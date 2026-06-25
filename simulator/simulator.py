"""
simulator.py — OpenRC-STM32 Ground Station / Simulator Link
============================================================
Reads the custom serial protocol produced by the transmitter's
Simulator Mode (USB CDC), decodes 6 analog + 2 digital RC
channels, and forwards them to a virtual joystick via vJoy.
Intended use: connect your OpenRC transmitter to a PC and use
it as a joystick input in any FPV simulator (e.g. Velocidrone,
Liftoff, MSFS).

Protocol summary (18-byte packet):
    [0x AA] [0xBB]          — 2-byte start marker
    [1 byte cmd]            — command/type byte
    [1 byte flags]          — reserved flags
    [6 × uint16_t LE]       — analog channels CH1–CH6  (0–4095)
    [1 byte aux_switches]   — bit0 = AUX3, bit1 = AUX4
    [1 byte CRC8]           — CRC over bytes 0–16

Author   : Александр Королёв
Version  : 1.7.0
Date     : 2026-05-17
License  : MIT
Project  : OpenRC-STM32  —  https://github.com/EbrSiami/OpenRC-STM32
Hardware : STM32F103C8 (Blue Pill) transmitter via USB CDC @ 115200 baud
"""

import os
import sys
import struct
import threading
import subprocess

import customtkinter as ctk
from tkinter import messagebox
import serial
import serial.tools.list_ports
import pyvjoy

# ──────────────────────────────────────────────────────────────
#  Theme
# ──────────────────────────────────────────────────────────────

ctk.set_appearance_mode("Dark")
ctk.set_default_color_theme("blue")

# ──────────────────────────────────────────────────────────────
#  CRC-8 (poly 0x07, init 0x00)
# ──────────────────────────────────────────────────────────────

def crc8_cal(data: bytes) -> int:
    """
    Calculate CRC-8 checksum (polynomial 0x07, initial value 0x00).

    Must produce identical results to the CRC routine in the
    transmitter firmware so that corrupt or partial packets are
    silently discarded rather than forwarded to vJoy.

    Args:
        data: Bytes to checksum (packet bytes 0–16).

    Returns:
        Single-byte CRC value (0x00–0xFF).
    """
    crc = 0x00
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ 0x07) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc


# ──────────────────────────────────────────────────────────────
#  Main Application
# ──────────────────────────────────────────────────────────────

class ModernRCApp(ctk.CTk):
    """
    OpenRC Ground Station application window.

    Responsibilities:
      - Enumerate and display available serial ports, highlighting
        known STM32 CDC devices.
      - Open a background thread that reads and parses serial packets.
      - Update the vJoy virtual joystick axes and buttons in real time.
      - Reflect channel values in the GUI with a noise-suppression
        threshold to avoid unnecessary redraws.
    """

    # ── Packet constants ──────────────────────────────────────
    PACKET_MARKER    = b'\xaa\xbb'   # 2-byte start-of-frame marker
    PACKET_SIZE      = 18            # total bytes per frame
    SERIAL_BAUD      = 115200
    BUFFER_FLUSH_THR = 128           # flush serial buffer if backlog exceeds this

    # GUI noise gate — don't redraw a channel bar unless the raw
    # ADC value changed by more than this amount (0–4095 scale).
    NOISE_THRESHOLD  = 3

    def __init__(self):
        super().__init__()

        self.title("EBR RC Controller Link v1.7")
        self.geometry("460x650")
        self.resizable(False, False)

        # Optional custom window icon (bundled with PyInstaller or local)
        if os.path.exists("logo.ico"):
            self.iconbitmap("logo.ico")

        # Serial / vJoy state
        self.serial_connection  = None
        self.running            = False
        self.vjoy_device        = None

        # Maps combo-box display strings → actual port device names
        self.available_ports_dict: dict[str, str] = {}

        # Previous channel values used for noise-gate comparison
        self.last_displayed_values = [0] * 6

        self._build_ui()
        self.refresh_ports()
        self._check_vjoy()

    # ── UI Construction ───────────────────────────────────────

    def _build_ui(self):
        """Construct and arrange all widgets."""

        # Title
        ctk.CTkLabel(self, text="RC GROUND STATION",
                     font=ctk.CTkFont(size=22, weight="bold")).pack(pady=(15, 5))

        # ── Connection frame ──
        conn = ctk.CTkFrame(self)
        conn.pack(pady=10, padx=20, fill="x")

        ctk.CTkLabel(conn, text="Select Port:").grid(row=0, column=0, padx=10, pady=10)

        self.port_combo = ctk.CTkComboBox(conn, values=[], width=180)
        self.port_combo.grid(row=0, column=1, padx=10, pady=10)

        ctk.CTkButton(conn, text="🔄", width=40,
                      command=self.refresh_ports).grid(row=0, column=2, padx=5, pady=10)

        self.connect_btn = ctk.CTkButton(
            conn, text="Connect Device",
            fg_color="#2ecc71", hover_color="#27ae60",
            command=self.toggle_connection)
        self.connect_btn.grid(row=1, column=0, columnspan=3,
                              padx=10, pady=10, sticky="we")

        # ── vJoy status indicator ──
        self.vjoy_status_lbl = ctk.CTkLabel(
            self, text="vJoy Status: Checking...",
            text_color="#f1c40f",
            font=ctk.CTkFont(size=12, weight="bold"))
        self.vjoy_status_lbl.pack(pady=5)

        # ── Channel bars (CH1–CH6 analog) ──
        ch_frame = ctk.CTkFrame(self)
        ch_frame.pack(pady=10, padx=20, fill="x")

        self.channel_progresses = []
        self.channel_labels     = []

        channel_names = [
            "Roll (CH1)", "Pitch (CH2)", "Throttle (CH3)",
            "Yaw (CH4)", "AUX 1 (CH5)", "AUX 2 (CH6)"
        ]
        for i, name in enumerate(channel_names):
            ctk.CTkLabel(ch_frame, text=name,
                         font=ctk.CTkFont(size=12)).grid(
                row=i, column=0, padx=15, pady=5, sticky="w")

            bar = ctk.CTkProgressBar(ch_frame, width=200)
            bar.set(0)
            bar.grid(row=i, column=1, padx=10, pady=5)
            self.channel_progresses.append(bar)

            val_lbl = ctk.CTkLabel(ch_frame, text="0", width=40)
            val_lbl.grid(row=i, column=2, padx=10, pady=5)
            self.channel_labels.append(val_lbl)

        # ── Digital switch indicators (CH7 / CH8) ──
        sw_frame = ctk.CTkFrame(self)
        sw_frame.pack(pady=10, padx=20, fill="x")

        self.sw3_lbl = ctk.CTkLabel(
            sw_frame, text="AUX 3 (CH7): DISARMED",
            fg_color="#c0392b", font=ctk.CTkFont(size=12, weight="bold"),
            corner_radius=6, height=35, width=190)
        self.sw3_lbl.grid(row=0, column=0, padx=10, pady=10)

        self.sw4_lbl = ctk.CTkLabel(
            sw_frame, text="AUX 4 (CH8): OFF",
            fg_color="#c0392b", font=ctk.CTkFont(size=12, weight="bold"),
            corner_radius=6, height=35, width=190)
        self.sw4_lbl.grid(row=0, column=1, padx=10, pady=10)

        # ── Driver installer ──
        ctk.CTkButton(self, text="⚡ Install Required Drivers",
                      fg_color="#34495e", hover_color="#2c3e50",
                      command=self.install_drivers).pack(
            pady=(15, 5), padx=20, fill="x")

        # ── Footer ──
        ctk.CTkFrame(self, height=2, fg_color="#2c3e50").pack(
            fill="x", padx=30, pady=10)

        info = ctk.CTkFrame(self, fg_color="transparent")
        info.pack(fill="x", padx=20, pady=(0, 10))

        ctk.CTkButton(info, text="ℹ EBR.co",
                      font=ctk.CTkFont(size=14, weight="bold"),
                      fg_color="transparent", text_color="#3498db",
                      hover_color="#2c3e50", width=80,
                      command=self.show_about).pack(side="left", padx=5)

        ctk.CTkLabel(info,
                     text="Hardware Link v1.7 | CRC Active",
                     font=ctk.CTkFont(size=11),
                     text_color="#7f8c8d").pack(side="right", padx=10)

    # ── Port Management ───────────────────────────────────────

    def refresh_ports(self):
        """
        Scan available serial ports and populate the combo box.

        Ports whose description contains STM32/CDC keywords are
        labelled as confirmed RC controllers; all others are marked
        as unknown so the user can still select them manually.
        """
        self.available_ports_dict.clear()
        combo_values = []

        for port in serial.tools.list_ports.comports():
            desc = port.description.lower()
            is_rc = any(kw in desc for kw in
                        ("stm", "cdc", "virtual", "stmicroelectronics"))
            label = (f"{port.device} (✓ RC Controller)"
                     if is_rc else f"{port.device} (⚠️ Unknown)")
            combo_values.append(label)
            self.available_ports_dict[label] = port.device

        self.port_combo.configure(values=combo_values)
        self.port_combo.set(combo_values[0] if combo_values else "No Port Found")

    # ── vJoy ─────────────────────────────────────────────────

    def _check_vjoy(self):
        """
        Attempt to open vJoy device #1 and update the status label.
        Called once at startup; the device is also re-opened on connect
        if it was unavailable at launch.
        """
        try:
            self.vjoy_device = pyvjoy.VJoyDevice(1)
            self.vjoy_status_lbl.configure(
                text="✓ vJoy Driver: Active & Ready", text_color="#2ecc71")
        except Exception:
            self.vjoy_status_lbl.configure(
                text="✗ vJoy Driver: Not Found/Error", text_color="#e74c3c")

    # ── Connection Toggle ─────────────────────────────────────

    def toggle_connection(self):
        """Connect or disconnect based on current state."""
        if not self.running:
            self.connect_btn.configure(
                text="Connecting...", fg_color="#d35400", state="disabled")
            threading.Thread(target=self._async_connect, daemon=True).start()
        else:
            self._stop_serial()

    def _async_connect(self):
        """
        Open the serial port and start the reader thread.
        Runs in a daemon thread to avoid blocking the GUI.
        """
        selected = self.port_combo.get()
        if not selected or selected == "No Port Found":
            self.after(0, self._on_connect_error, "Please select a valid port.")
            return

        port_name = self.available_ports_dict.get(selected, selected.split()[0])

        try:
            self.serial_connection = serial.Serial(
                port_name, self.SERIAL_BAUD, timeout=0.5)

            # Try to acquire vJoy if it wasn't available at launch
            if not self.vjoy_device:
                self.vjoy_device = pyvjoy.VJoyDevice(1)

            self.running = True
            self.after(0, self._on_connect_success)

            threading.Thread(target=self._read_loop, daemon=True).start()

        except serial.SerialException as exc:
            msg = "Failed to connect.\n\n"
            msg += ("Port is in use by another application."
                    if "PermissionError" in str(exc) or "Access is denied" in str(exc)
                    else "Check the USB cable connection.")
            self.after(0, self._on_connect_error, msg)
        except Exception as exc:
            self.after(0, self._on_connect_error, f"Unexpected error:\n{exc}")

    def _on_connect_success(self):
        self.connect_btn.configure(
            text="Disconnect Device",
            fg_color="#e74c3c", hover_color="#c0392b", state="normal")

    def _on_connect_error(self, message: str):
        self.connect_btn.configure(
            text="Connect Device",
            fg_color="#2ecc71", hover_color="#27ae60", state="normal")
        messagebox.showerror("Connection Failed", message)

    def _stop_serial(self):
        """Close the serial port and reset the GUI to idle state."""
        self.running = False
        if self.serial_connection and self.serial_connection.is_open:
            self.serial_connection.close()

        self.connect_btn.configure(
            text="Connect Device",
            fg_color="#2ecc71", hover_color="#27ae60", state="normal")

        # Reset channel displays
        for bar in self.channel_progresses:
            bar.set(0)
        for lbl in self.channel_labels:
            lbl.configure(text="0")
        self.sw3_lbl.configure(text="AUX 3 (CH7): DISARMED", fg_color="#c0392b")
        self.sw4_lbl.configure(text="AUX 4 (CH8): OFF",      fg_color="#c0392b")

    # ── Serial Reader Thread ──────────────────────────────────

    def _read_loop(self):
        """
        Background thread: read bytes from serial, locate packet
        markers, verify CRC, and dispatch decoded channel data.

        Packet format (18 bytes):
            Offset  Size  Description
            0       2     Start marker 0xAA 0xBB
            2       1     Command byte
            3       1     Flags byte
            4       12    6 × uint16_t LE  (CH1–CH6, range 0–4095)
            16      1     aux_switches byte  (bit0=AUX3, bit1=AUX4)
            17      1     CRC8 over bytes 0–16

        The buffer is flushed if backlog exceeds BUFFER_FLUSH_THR bytes
        to prevent latency buildup (e.g. on a slow machine).
        """
        buffer = bytearray()

        if self.serial_connection and self.serial_connection.is_open:
            self.serial_connection.reset_input_buffer()

        while self.running:
            try:
                if not (self.serial_connection and self.serial_connection.is_open):
                    break

                waiting = self.serial_connection.in_waiting
                if waiting == 0:
                    continue

                # Drop buffer if too far behind — prefer fresh data
                if waiting > self.BUFFER_FLUSH_THR:
                    self.serial_connection.reset_input_buffer()
                    buffer.clear()
                    continue

                buffer.extend(self.serial_connection.read(waiting))

                # Parse all complete packets in the buffer
                while self.PACKET_MARKER in buffer:
                    start = buffer.find(self.PACKET_MARKER)

                    if len(buffer) < start + self.PACKET_SIZE:
                        break  # Wait for more bytes

                    packet = buffer[start: start + self.PACKET_SIZE]

                    # Verify CRC (covers all bytes except the last)
                    if crc8_cal(packet[:17]) == packet[17]:
                        # Unpack: 2B marker + 2B header + 6×H channels + B switches + B crc
                        decoded      = struct.unpack('<BBBBHHHHHHBB', packet)
                        channels     = decoded[4:10]   # tuple of 6 uint16 values
                        aux_switches = decoded[10]     # bitmask: bit0=AUX3, bit1=AUX4

                        self._forward_to_vjoy_and_gui(channels, aux_switches)

                    # Advance past this packet regardless of CRC result
                    buffer = buffer[start + self.PACKET_SIZE:]

            except Exception:
                self.after(0, self._on_runtime_disconnect)
                break

    def _on_runtime_disconnect(self):
        """Handle unexpected serial loss during active session."""
        if self.running:
            self._stop_serial()
            messagebox.showwarning(
                "Connection Lost",
                "RC transmitter disconnected!\nCheck the USB cable.")

    # ── vJoy + GUI Update ─────────────────────────────────────

    def _forward_to_vjoy_and_gui(self, channels: tuple, aux_switches: int):
        """
        Map decoded RC channel values to vJoy axes/buttons, then
        schedule a GUI update on the main thread.

        Channel → vJoy axis mapping:
            CH1 (Roll)     → X
            CH2 (Pitch)    → Y
            CH3 (Throttle) → Z
            CH4 (Yaw)      → Rx
            CH5 (AUX1)     → Ry
            CH6 (AUX2)     → Rz

        Raw ADC range 0–4095 is scaled to vJoy range 0–32767.

        Args:
            channels:     Tuple of 6 uint16 analog channel values.
            aux_switches: Bitmask byte; bit0 = AUX3, bit1 = AUX4.
        """
        axis_map = [
            pyvjoy.HID_USAGE_X,  pyvjoy.HID_USAGE_Y,  pyvjoy.HID_USAGE_Z,
            pyvjoy.HID_USAGE_RX, pyvjoy.HID_USAGE_RY, pyvjoy.HID_USAGE_RZ,
        ]

        if self.vjoy_device:
            for i, val in enumerate(channels[:6]):
                self.vjoy_device.set_axis(
                    axis_map[i], int((val / 4095.0) * 32767))

            a3 = 1 if (aux_switches & (1 << 0)) else 0
            a4 = 1 if (aux_switches & (1 << 1)) else 0
            self.vjoy_device.set_button(1, a3)
            self.vjoy_device.set_button(2, a4)
        else:
            a3, a4 = 0, 0

        # GUI updates must happen on the main thread
        self.after(0, self._update_gui, channels, a3, a4)

    def _update_gui(self, channels: tuple, a3: int, a4: int):
        """
        Refresh channel progress bars and switch indicators.

        A noise gate (NOISE_THRESHOLD) prevents flickering redraws
        when ADC values drift by only 1–2 counts at rest.
        Edges (0 and 4095) are always redrawn to ensure full-scale
        positions are reflected correctly.

        Args:
            channels: Tuple of 6 uint16 analog values.
            a3, a4:   Digital switch states (0 or 1).
        """
        for i, val in enumerate(channels):
            if (abs(val - self.last_displayed_values[i]) > self.NOISE_THRESHOLD
                    or val in (0, 4095)):
                self.channel_progresses[i].set(val / 4095.0)
                self.channel_labels[i].configure(text=str(val))
                self.last_displayed_values[i] = val

        # AUX3 — typically used as ARM switch
        if a3:
            self.sw3_lbl.configure(text="AUX 3 (CH7): ARMED",    fg_color="#2ecc71")
        else:
            self.sw3_lbl.configure(text="AUX 3 (CH7): DISARMED", fg_color="#c0392b")

        if a4:
            self.sw4_lbl.configure(text="AUX 4 (CH8): ON",  fg_color="#2ecc71")
        else:
            self.sw4_lbl.configure(text="AUX 4 (CH8): OFF", fg_color="#c0392b")

    # ── About Dialog ──────────────────────────────────────────

    def show_about(self):
        """Open a modal About window with project and contact info."""
        win = ctk.CTkToplevel(self)
        win.title("About — OpenRC-STM32")
        win.geometry("400x320")
        win.resizable(False, False)
        win.transient(self)
        win.grab_set()

        ctk.CTkLabel(win, text="OpenRC-STM32  Ground Station",
                     font=ctk.CTkFont(size=18, weight="bold"),
                     text_color="#3498db").pack(pady=(20, 10))

        ctk.CTkLabel(win, justify="center", font=ctk.CTkFont(size=12), text=(
            "Designed & developed by Александр Королёв (EBR.co)\n\n"
            "Bridges your OpenRC transmitter to any FPV simulator\n"
            "via a virtual joystick (vJoy).\n\n"
            "• STM32 USB CDC serial protocol\n"
            "• 6 analog + 2 digital channels\n"
            "• Hardware CRC-8 packet verification"
        )).pack(pady=10)

        ctk.CTkLabel(win,
                     text="Support: +xx xxxxxxxxxx  |  Telegram: @xxxxxx",
                     font=ctk.CTkFont(size=11, slant="italic"),
                     text_color="#2ecc71").pack(pady=(15, 10))

        ctk.CTkButton(win, text="Close", width=100,
                      command=win.destroy).pack(pady=10)

    # ── Driver Installer ──────────────────────────────────────

    def install_drivers(self):
        """
        Open the bundled 'drivers' folder so the user can manually
        install vJoy and the STM32 Virtual COM Port driver.

        Works both when running from source and when packaged with
        PyInstaller (sys._MEIPASS points to the temp extraction dir).
        """
        base = getattr(sys, '_MEIPASS', os.path.dirname(os.path.abspath(__file__)))
        drivers_dir = os.path.join(base, "drivers")

        if os.path.exists(drivers_dir):
            try:
                messagebox.showinfo(
                    "Drivers Folder",
                    "Opening drivers folder.\n"
                    "Install 'vJoy' and 'STM32 Virtual COM Port' if not already done.")
                os.startfile(drivers_dir)
            except Exception as exc:
                messagebox.showerror("Error", f"Could not open folder:\n{exc}")
        else:
            messagebox.showerror(
                "Missing Files",
                "Drivers folder not found.\n"
                "Re-download the release package from the GitHub Releases page.")


# ──────────────────────────────────────────────────────────────
#  Entry Point
# ──────────────────────────────────────────────────────────────

if __name__ == "__main__":
    app = ModernRCApp()
    app.mainloop()