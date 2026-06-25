# OpenRC-STM32

![Stand With Iran](https://img.shields.io/badge/Stand_With-Iran_🇮🇷-green?color=red)
![License](https://img.shields.io/badge/license-MIT-green)
![PlatformIO](https://img.shields.io/badge/build-PlatformIO-orange?logo=platformio)
![STM32](https://img.shields.io/badge/MCU-STM32-blue?logo=stmicroelectronics)
![NRF24](https://img.shields.io/badge/radio-NRF24L01+-yellow)
![Transmitter](https://img.shields.io/badge/firmware-v4.0.1--beta-cyan)
![Receiver](https://img.shields.io/badge/receiver-v1.3.2-cyan)
![Simulator](https://img.shields.io/badge/simulator-v1.7.0-brightgreen)

**An open-source, STM32-based RC radio system** — transmitter firmware, receiver firmware,
and a PC ground station for simulator use. Everything you need to build a fully custom
radio control link from scratch.

> 📖 **README in other languages:** [فارسی](docs/README_FA.md) · [Русский](docs/README_RU.md)

---

## What's in this repository

| Component | Target Hardware | Version | Status |
| :--- | :--- | :--- | :--- |
| [Transmitter firmware](#transmitter) | STM32F103C**B**T6 / 128 KB clone | v4.0.1-beta | Beta |
| [Receiver firmware](#receiver) | STM32G030F6P6 | v1.3.2 | Stable |
| [RC Ground Station (Simulator)](#simulator) | Windows PC | v1.7.0 | Stable |

Pre-built binaries (`.bin`, `.elf`) and the Ground Station installer (`.exe`, drivers included)
are available on the [**Releases page**](../../releases).

---
## Terms of Use & Ethical Clause

By cloning, downloading, using, or contributing to this repository, you explicitly acknowledge, agree to, and align with the following conditions. If you do not support these principles, you are not authorized to use this software.

### 1. Condemnation of External Aggression & War Crimes
* **Opposition to Invasions:** You strictly condemn any military aggression, strikes, or violation of territorial integrity against Iran by the United States, Israel, or any foreign coalition, emphasizing that sovereign nations must be free from external military interventions.
* **Acknowledgment of the 39-Day War:** You recognize the devastating impacts of the 39-Day War, which resulted in the loss of thousands of innocent lives, the destruction of critical civilian infrastructure, and over $300 billion in direct economic damages.
* **Civilian Casualties:** You honor the memory of all civilian victims, including specific tragic atrocities such as the loss of 168 school children at the Minab school bombing, and condemn any military doctrines that treat civilian lives as collateral damage.

### 2. Accountability for Domestic Governance & Civil Rights
* **Systemic Inflation & Economic Policies:** You reject state-driven economic decisions that destroy public livelihood, specifically the abrupt elimination of preferential currency exchange rates (Arz-e Tarjihi), which caused hyperinflation and pushed millions into severe poverty.
* **Suppression of Protests:** You condemn the violent state crackdowns on peaceful civil protests, including the tragic events of January protests, demanding full accountability for the state-sponsored killing of innocent citizens.
* **Digital Censorship & Internet Blackouts:** You oppose the regime's pervasive filtering systems and the implementation of prolonged internet blackouts—such as the historic 88-day total digital shutdown—which isolate citizens from the global community and violate the fundamental right to free information.
* **Ideological & Social Enforcement:** You reject the forced imposition of strict state ideologies, mandatory social restrictions, and the prioritization of ideological agendas over the welfare, freedom, and human rights of the Iranian people.

---
#### This clause is a non-negotiable condition of license. Using this code while justifying or turning a blind eye to either foreign military destruction or domestic systemic oppression is a violation of this software's terms.
---

<a name="transmitter"></a>
## 🎮 Transmitter

8-channel RC transmitter firmware for the STM32F103.
Runs a **500 Hz control loop** with a full OLED menu system — no PC required for configuration.

### Features

**Control & Input**
- 8 channels: 4 analog gimbals + 2 analog pots + 2 digital switches
- Digital trims for Roll, Pitch, and Yaw — saved to EEPROM with center-snap and end limits
- Selectable throttle mode: Airplane (lower-half zero) or Normal

**Signal Processing**
- EMA noise filter on all 6 analog inputs
- Step-by-step stick calibration wizard (center, min, max)
- Configurable deadband, Expo (−100 % to +100 %), Dual Rate (10–100 %)
- Sub-Trim and EPA (End-Point Adjustment) for the 3 main channels
- Per-channel inversion (all 8 channels)
- 5 mixer presets: Normal, V-Tail A/B, Delta A/B

**OLED UI (0.96")**
- Real-time dashboard: battery voltage, flight timer, D/R status, TX link indicator
- Live channel bar views (CH1–4 and CH5–8)
- Flight timer: countdown or stopwatch, armed by throttle, with buzzer alerts at 1 min / 30 s / last 10 s
- Full menu system: Expo, Dual Rate, Channel Invert, Mixer, Calibration, EPA/Sub-Trim, Simulator Mode
- Animated splash screen at boot

**Reliability**
- Non-blocking architecture — zero `delay()` in the main loop
- NRF24L01+ at 250 kbps, max power, auto-ACK off — 500 Hz update rate
- Priority buzzer engine (14 patterns) — high-priority alarms override all settings
- EEPROM with magic number + CRC; auto-reset to safe defaults on corruption
- 2S/3S LiPo battery monitor with low-voltage SOS alarm

### ⚠️ Flash Size — Important

The compiled firmware is approximately **70 KB**. The standard STM32F103C**8**T6
(Blue Pill, 64 KB Flash) is **not supported**. Use one of the following:

- STM32F103C**B**T6 (128 KB Flash) — recommended
- STM32F103 Chinese clones with 128–256 KB Flash (common, usually works)
- STM32 Black Pill (with pinout adjustments)

### Pinout

| Component | STM32 Pin | Notes |
| :--- | :--- | :--- |
| **NRF24L01+** | | |
| CE | `PB8` | |
| CSN | `PB9` | |
| SCK / MISO / MOSI | `PA5 / PA6 / PA7` | SPI1 |
| **OLED Display** | | |
| SDA / SCL | `PB7 / PB6` | I2C1 |
| **Gimbals** | `PA0 – PA3` | ADC |
| **Aux Pots** | `PB0, PB1` | ADC |
| **Aux Switches** | `PB4, PB5` | Digital input |
| **Trim Buttons** | `PB15, PA8–PA10, PA15, PB3` | Active low |
| **Nav Buttons** | `PB12 – PB14` | Up / Down / Enter |
| **Buzzer** | `PC13` | Active high |
| **Battery Sense** | `PA4` | Voltage divider |

### Hardware Notes

**NRF24L01+ power supply**
The module is sensitive to power noise. Solder a 10–100 µF capacitor directly across its
VCC and GND pins. A dedicated AMS1117-3.3 regulator is preferred over the Blue Pill's
onboard 3.3 V rail, which may not supply enough peak current.

**I2C pull-ups**
The STM32 internal pull-ups are too weak for I2C. Add external 4.7 kΩ resistors on
both SDA and SCL.

**Buzzer driver**
Do not connect the buzzer directly to a GPIO pin. Use an NPN transistor (e.g. 2N2222)
or a small MOSFET as a driver circuit.

**Battery voltage divider**
R1 = 22 kΩ (to Battery+), R2 = 6.8 kΩ (to GND). Maximum measurable voltage: ~14 V (3S safe).

### Build & Flash

**Prerequisites:** VS Code with PlatformIO extension, ST-Link V2 programmer.

```bash
git clone https://github.com/EbrSiami/OpenRC-STM32.git
```

Open the `transmitter/` folder in PlatformIO, build with **✓**, and upload with **→**.

Alternatively, flash the pre-built `.elf` from the Releases page using STM32CubeProgrammer
(preferred over `.bin` for automatic address handling).

---

<a name="receiver"></a>
## 📡 Receiver

Compact 8-channel receiver firmware. Decodes NRF24L01+ packets from the transmitter
and outputs both individual **PWM signals** and a hardware-timed **PPM stream** simultaneously.
Includes a 500 ms failsafe that cuts throttle on link loss.

**Compile with `-Os`** (optimize for size) — the G030F6P6 has 32 KB Flash and 8 KB RAM.

### LED Status

| Pattern | Meaning |
| :--- | :--- |
| Solid ON | Link active — packets being received |
| Slow blink (500 ms) | Waiting for signal |
| Fast blink (100 ms) | NRF24L01+ hardware fault |

### Failsafe Values

On link loss (> 500 ms), all channels are set to safe defaults:
Roll / Pitch / Yaw → 1500 µs (neutral), Throttle → 900 µs (motors off),
all Aux channels → 1000 µs.

### Pinout

| Component | STM32 Pin | Notes |
| :--- | :--- | :--- |
| **NRF24L01+** | | |
| CE | `PA4` | |
| CSN | `PA5` | |
| SCK / MISO / MOSI | `PA1 / PA6 / PA2` | SPI |
| **PWM Outputs** | | |
| CH1 Roll | `PA0` | |
| CH2 Pitch | `PA3` | |
| CH3 Throttle | `PA7` | |
| CH4 Yaw | `PB0` | |
| CH5 Aux1 | `PA11` | |
| CH6 Aux2 | `PA12` | |
| CH7 Aux3 | `PB3` | |
| CH8 Aux4 | `PB7` | |
| **PPM Output** | `PC14` | Hardware timer TIM3 |
| **Status LED** | `PC15` | |

### Hardware Notes

**NRF24L01+ power supply**
Same as transmitter — add a 10–100 µF capacitor directly across the module's VCC and GND.

**RST and BOOT0 pins**
These must not be left floating. Pull BOOT0 to GND (normal run mode).
RST should have a pull-up resistor to VCC (typically 10 kΩ).

**No external crystal needed**
The G030F6P6 uses its internal RC oscillator. No external crystal is required.

**PWM / PPM signal lines**
Add a ~1 kΩ series resistor on each PWM and PPM output pin to protect the MCU
and reduce ringing on long cables.

### Build & Flash

Open the `receiver/receiver.ino` file in Arduino IDE. Set `build_flags to -Os` in Menu.

Or Flash using ST-Link V2 with STM32CubeProgrammer (use the `.elf` from Releases).

---

<a name="simulator"></a>
## 🖥️ Simulator (RC Ground Station)

A Windows desktop application that bridges the transmitter's **Simulator Mode**
to any FPV simulator via a **vJoy virtual joystick**.

### How it works

The transmitter's Simulator Mode disables the NRF24L01+ radio and instead sends
channel data over the STM32's built-in USB CDC (Virtual COM Port).

> **Note:** This uses a custom serial protocol — not iBUS or SBUS.
> Standard iBUS timing was not reliable over STM32 USB CDC (which behaves differently
> from dedicated USB-UART chips like CH340), so a purpose-built protocol was developed
> for full control over framing and error detection. The protocol includes a
> **CRC-8 checksum** on every packet.

Simulator Mode is **never saved to EEPROM** — the transmitter always boots with the
radio active. This is intentional for safety.

### Activation — step by step

1. Power on the transmitter (radio is active by default).
2. Navigate to **Features** in the OLED menu.
3. Select **Simulator Mode** (last item, default: `Off`) and turn it `On`.
4. The NRF24L01+ shuts down automatically and the transmitter switches to USB data output.
5. Connect the transmitter to your PC via USB.
6. Open **RC Ground Station**, select the correct COM port (STM32 CDC ports are marked ✓),
   and click **Connect Device**.
7. Channel bars will update in real time. The transmitter is now a joystick.

**Before connecting:** install vJoy and the STM32 Virtual COM Port driver.
Both are included in the Ground Station installer, or can be opened via
the **⚡ Install Required Drivers** button inside the app.

### Channel Mapping

| RC Channel | vJoy Axis | Range |
| :--- | :--- | :--- |
| CH1 Roll | X | 0–32767 |
| CH2 Pitch | Y | 0–32767 |
| CH3 Throttle | Z | 0–32767 |
| CH4 Yaw | Rx | 0–32767 |
| CH5 AUX1 | Ry | 0–32767 |
| CH6 AUX2 | Rz | 0–32767 |
| CH7 AUX3 | Button 1 | 0 / 1 |
| CH8 AUX4 | Button 2 | 0 / 1 |

### Running from source

```bash
pip install customtkinter pyserial pyvjoy
python simulator/simulator.py
```

The pre-built `.exe` (with bundled drivers) is available on the [Releases page](../../releases).

---

## 📦 Releases

Each release includes:

| File | Description |
| :--- | :--- |
| `transmitter_vX.X.X.elf` | Transmitter firmware — flash with STM32CubeProgrammer |
| `transmitter_vX.X.X.bin` | Transmitter firmware — raw binary |
| `receiver_vX.X.X.elf` | Receiver firmware — flash with STM32CubeProgrammer |
| `receiver_vX.X.X.bin` | Receiver firmware — raw binary |
| `RC_Ground_Station_vX.X.X.exe` | Windows Simulator App with vJoy + STM32 drivers bundled |

> **Recommended:** Use `.elf` files with STM32CubeProgrammer. ELF files include
> flash address metadata, which prevents accidental writes to the wrong memory region.

---

## 📂 Repository Structure

```
OpenRC-STM32/
├── transmitter/          # PlatformIO project — STM32F103
│   ├── lib/                        # External libraries
│   ├── src/                        # Source Code & Headers
│   │   ├── main.cpp                # Entry point & Main Loop
│   │   ├── Radio.cpp/.h            # NRF24L01 Driver & Logic
│   │   ├── DisplayManager.cpp/.h   # OLED UI & Graphics Engine
│   │   ├── Button.cpp/.h           # Non-blocking Input Handler
│   │   ├── buzzer.cpp/.h           # buzzer handling engine
│   │   ├── sim_protocol.cpp/.h     # Simulator data protocol
│   │   └── Settings.h              # Global Configuration Structs
│   └── platformio.ini
├── receiver/             # Arduino IDE project — STM32G030
│   └── receiver.ino
├── simulator/            # Python — RC Ground Station
│   ├── drivers/                    # needed drivers for USB CDC and vJoy
│   ├── airplane.ico                # windows simulator app icon
│   └── simulator.py                # main source file for windows app
├── docs/                 # future Diagrams and README's
│   ├── README_FA.md                # RUSSIAN translation
│   ├── README_RU.md                # Persian translation
│   └── Photos/                     # some photos of my DIY RC Build
└── README.md
```

---

## 🔧 Third-Party Libraries

- [`Adafruit_SSD1306`](https://github.com/adafruit/Adafruit_SSD1306) — OLED display driver
- [`RF24`](https://github.com/nRF24/RF24) — NRF24L01+ radio driver
- [`FlashStorage_STM32`](https://github.com/khoih-prog/FlashStorage_STM32) — EEPROM emulation on STM32 Flash
- [`customtkinter`](https://github.com/TomSchimansky/CustomTkinter) — Modern Python GUI toolkit
- [`pyvjoy`](https://github.com/tidzo/pyvjoy) — vJoy virtual joystick interface
- [`pyserial`](https://github.com/pyserial/pyserial) — Serial communication

---

## ❤️ Dedication

**To Marya.**

This project carries your fingerprints on its core logic. Thank you for your patience,
your contributions, and for the hidden messages I found too late.
You were the co-pilot of this project, even when I was flying solo.

*Милая Маша, я даже не знаю, как тебя благодарить за то, что ты уделила столько времени
такому дураку, как я. Мне очень жаль, что я не могу сделать для тебя большего — но знай:
я твой вечный должник. Я и подумать не мог, что мы вместе доведём эту прошивку до такого
уровня. Спасибо тебе ещё раз за всё.*

*Прости, что мне скоро придётся уйти — но будь уверена, я тебя никогда не забуду.* 🕊️

---

**Lead Developer:** Александр Королёв
**Core Logic & Inspiration:** Мария ... :)

*Open-source under the [MIT License](LICENSE).*