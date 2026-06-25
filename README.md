# STM32 Advanced RC Transmitter 🎮📡

![PlatformIO](https://img.shields.io/badge/PlatformIO-Build-orange?logo=platformio)
![STM32](https://img.shields.io/badge/Hardware-STM32F103-blue?logo=stmicroelectronics)
![C++](https://img.shields.io/badge/Code-C%2B%2B-00599C?logo=c%2B%2B)
![Version](https://img.shields.io/badge/version-4.0.1-cyan)
![License](https://img.shields.io/badge/license-MIT-green)
![Build](https://img.shields.io/badge/build-passing-brightgreen)
![NRF24](https://img.shields.io/badge/radio-NRF24L01+-yellow?logo=nordicsemiconductor)
![OLED](https://img.shields.io/badge/display-OLED_0.96"-white)
![Rate](https://img.shields.io/badge/update-500Hz-red)

**A professional-grade 8-channel RC transmitter firmware for the STM32F103C8 (Blue Pill).**
Combines a **500 Hz real-time control loop** with a responsive OLED UI, 
advanced channel processing (EMA filter, Expo, Dual Rate, EPA, Sub-Trim), 
and 5 built-in mixes including V-Tail and Delta.  
Features a non‑blocking priority‑based sound engine, 
and reliable NRF24L01+ communication at 250 kbps with live link monitoring.  
Designed for FPV drones, airplanes, and custom robotics – 
fully configurable via onboard menus, no PC needed.

---

## ✨ Features

### 🎮 Control & Input
- **8 Channels:** 4 Analog (Gimbals) + 2 Analog (Pots) + 2 Digital (Switches).
- **Digital Trims:** Persistent trim adjustment for Roll, Pitch, and Yaw saved to EEPROM with center snap and end limits.
- **Throttle Mode:** Selectable Airplane (zero lower half) or Normal mode.

### 🧠 Advanced Signal Processing
- **EMA Noise Filter:** Fast exponential moving average on all analog inputs.
- **Calibration:** Step-by‑step wizard – stores center, min, and max for each stick.
- **Deadband:** Configurable dead zone around center to suppress jitter.
- **Expo:** Adjustable percent (-100% to +100%) for Roll, Pitch, and Yaw (cubic formula).
- **Dual Rate:** Separate percentage sensitivity (10–100%) per primary axis.
- **Sub-Trim & EPA:** Fine‑tune center offset and end‑point limits for 3 main channels.
- **Channel Inversion:** Reverse any of the 8 channels individually.
- **Mixing:** 5 pre‑defined mixes – Normal, V-Tail A/B, Delta A/B.

### 🖥️ User Interface (OLED 0.96")
- **Real-time Dashboard:** Battery voltage, timer, D/R status, and radio link indicator.
- **Channel Bars:** Live 1–4 and 5–8 channel views.
- **Flight Timer:** Countdown/Count‑up, armed by throttle, with buzzer alerts (1min, 30s, last 10s, finished).
- **Full Menu System:**
  - Light/Dark Mode, Buzzer on/off, Throttle type, Reset Trims, About.
  - **Advanced sub‑menus:** Expo, Dual Rate, Channel Invert, Mixer, Calibration, Channel Config (EPA/Sub‑trim).
- **Simulator Mode Toggle:** Disable radio and send formatted data over USB.
- **Splash Screen:** Animated MIG‑21 jet with loading bar.

### ⚙️ Hardware & Reliability
- **Non-blocking Core:** State machines for buttons, buzzer, timer, and display – zero `delay()`.
- **Battery Monitor:** 2S/3S LiPo via ADC, filtered, with low‑voltage SOS alarm.
- **High‑Speed Radio:** NRF24L01+ at 250kbps, max power, auto‑ack off – 500Hz update rate.
- **Radio Status Monitoring:** Live TX OK/Error indication on OLED.
- **Priority Buzzer Engine:** 14 distinct patterns; high‑priority alarms (battery, timer done) override settings.
- **Robust Storage:** EEPROM with magic number + checksum; auto‑reset to safe defaults on corruption.

---

## 📝 Changelog

### v4.0.1-beta A Major Upgrade (Current)

- **Auto Repeat:** for changing values like timer (between 0-60 min) or scrolling in menus, i have fully updated the botton files so now you just need to press and hold the down/up botton!
- **Advanced Buzzer:** i have completely upgraded the buzzer system. the crappy old beep function was using ```delay(duration_ms);``` so imagine im in flying and the timer reaches. it beeps for 5 seconds. you lose the plane controll for 5 seconds lmao. 
so the new system is fully modular in ```buzzer.cpp, buzzer.h`` files, its using millis. it supports priority. and it support a lot of new profiles for each job. i will use a non active buzzer in future
- **battery monitoring:** so you know the ```VOLTAGE_CONVERSION_FACTOR = (ADC_MAX_VOLTAGE / 4095.0) * ((R1 + R2) / R2) * CORRECTION_FACTOR``` formula is now calculated while compiling and loop is a bit lighter. 
i have also added a EMA filter for battery voltage and its updating every 250ms so its much more stable now
also the min and max voltages are updated now.
- **Main Menu (PAGE_MAIN3): well, timer is updated and now you can turn off/on the dual rate via D/R botton. there is also a TX status indicator which shows you that is the NRF24 chip is connected and analyzed or no (TX: OK, TX: ERR).
- **EXPO (Exponential):** its not much advanced yet but usable and you can set the positive (for airplanes) or negative (for cars or quad) expo in settings by setting a percent between -100 and 100%. the expo is only available for 3 main channels (Roll, Elevator, Rudder), the system is also showing you live sticks in bars i added there.
- **Dual Rate:** you can set the 3 main channels dual rate by a percent between 10 and 100. there is bars to showing channels just like expo. 
- **Channel Advanced:** here you can set the channels mid value and max, min value for 3 main channels (EPA and Subtrim)i wanted to destroy my pc while adding this. that was really hard. 
- **Calibration:** you can calibrate the sticks min, max and center values in calibration manu.
- **Channels Mix:** i have also added a channel mix for delta wing aircrafts that you can use. also it supports V-Tail aircraft like some drones. we have delta-a and delta-b which delta-b is invert channels for some airplens, also for v-tail.
- **Simulator Mode:** i tried to use ibus protocol to send stick datas in STM32 USB CDC but duo to the Different behavior of CDC than other USB/TTL chips, it was almost impossible so i developed my own protocol for more controll which i will explain it in a different repository. you can turn on simulator mode and use your radiocontroll as a joystick to pratice flying. it will turn off the NRF24 chip.
- **Footer:** its not much important but i added a fixed Footer function for navigation.
- **EMA Filter:** we have a light and simple EMA on 6 main analog channels for more stability. its using 2 factor (50% new data and 50% old data), you can change it in global variables.
- **Deadband:** for removing natural sticks unstable mid value.
- **Timer/Stopwatch:** you can set a timer between 0 and 60 minutes. the timer is armed when you use throttle. and if you set timer to 00:00 it will count up like a stopwatch which is very usable.
also when timer is finished it will tell you passed time with -00:00 (like -00:05 if 5 seconds passed after timer reached)
- **Timer Beeps:** the timer will 2 beeps when 1 min remaining, 1 beep when 30 seconds and beep every second in last 10 seconds. then a long beep when timer reached. and alert you with small beeps every 5 seconds after timer expired.
- **Buzzer Profiles:** welcome beep when system started after splash screen, And different sounds for confirmation, cancellation, trims, midpoint, end, etc.
- **EEPROM:** the external EEPROM chip destroyed me and it was not really stable so im using STM32 flash as eeprom. i have updated the RadioSettings for all parameters and its using a Magic Number plus supporting for CRC. also im turning off interrupts while saving. the whole savesettings and loadsettings function fully updated.
- **500Hz sending data frequancy:** the system is lighter now and its reading sticks values every 2ms and sending packets every 2ms (500hz);
- **Blinking:** if youre changing a vlue like timer or expo, dual rate. it blinks (generally if its in edit mode)
- **Dynamic Refresh rate:** now if youre in main menu, system will update screen every 200ms (fps) and in other menus its around 25fps (40ms) cuz we use i2c and it takes around 40ms for updating screen. you must be in main menu while flying. i will add a function that backs to main menu after a few seconds you not used screen in future.
- **Trim:** its more advanced now and changing value every 100ms while its only beeps every 700ms.

## oufff there is still a few more things but i really not remember. thanks!
### sorry if im too talkative.

### v2.6.1 - The "Performance" Update
- **🚀 Airplane Mode:** Added Smart Throttle logic (Split-stick behavior) for fixed-wing aircraft vs. Quadcopters.
- **⚡ Low Latency:** Optimized radio loop to run at **100Hz (10ms)** by prioritizing packet transmission over display refresh.
- **🔄 Channel Inverter:** New dedicated menu to reverse channel direction via software (saved to EEPROM).
- **🔋 Power Optimization:** Fixed OLED refresh bottleneck, allowing the PA (Power Amplifier) to draw full current for maximum range.
- **🎮 Improved UX:** Hold-to-adjust logic added for digital trims.

### v2.1.3 - Initial Release
- Basic 8-channel control, OLED dashboard, and NRF24L01 support.

---

## 📂 Project Structure

```text
STM32-RC-Transmitter/
├── include/              # Header files (if separated)
├── lib/                  # External libraries
├── src/                  # Source Code & Headers
│   ├── main.cpp          # Entry point & Main Loop
│   ├── Radio.cpp/.h      # NRF24L01 Driver & Logic
│   ├── DisplayManager... # OLED UI & Graphics Engine
│   ├── Button.cpp/.h     # Non-blocking Input Handler
│   ├── Settings.h        # Global Configuration Structs
│   ├── sim_protocol.c... # Simulator data protocol
│   ├── buzzer.cpp/.h     # buzzer handling engine
│   └── Settings.h        # Global Configuration Structs
├── test/                 # Unit testing (PlatformIO default)
├── platformio.ini        # Build & Board Configuration
├── LICENSE               # MIT License
└── README.md             # Documentation
```
---

## 🔌 Pinout Configuration

| Component | STM32 Pin | Description |
| :--- | :--- | :--- |
| **Radio (NRF24)** | | |
| CE | `PB8` | Chip Enable |
| CSN | `PB9` | Chip Select Not |
| SCK/MISO/MOSI | `PA5/PA6/PA7` | SPI1 Bus |
| **Display (OLED)** | | |
| SDA / SCL | `PB7 / PB6` | I2C1 Bus |
| **Controls** | | |
| Gimbals (A/E/T/R) | `PA0-PA3` | ADC Inputs |
| Aux Pots | `PB0, PB1` | ADC Inputs |
| Aux Switches | `PB4, PB5` | Digital Inputs |
| **Buttons** | | |
| Trims | `PB15, PA8-PA10, PA15, PB3` | Active Low |
| Navigation | `PB12-PB14` | Up/Down/Enter |
| **Misc** | | |
| Buzzer | `PC13` | Active High |
| V-Sense | `PA4` | Voltage Divider Input |

---

## 🔌 Hardware Setup Notes

For stable operation, please ensure the following hardware configurations:

### 1. NRF24L01+ Module
- **Power:** This module is extremely sensitive to power noise.
  - Solder a **10µF to 100µF capacitor** directly across the VCC and GND pins of the module.
  - Use a dedicated 3.3V regulator (like AMS1117-3.3) if possible, as the STM32's onboard 3.3V might not provide enough peak current.

### 2. I2C Bus (Display & EEPROM)
- **Pull-up Resistors:** The STM32 Blue Pill requires external pull-up resistors (4.7kΩ) on `SCL` and `SDA` lines for both I2C1 and I2C2 buses, as internal pull-ups are weak.

### 3. Buzzer
- **Driver Circuit:** Do not connect the buzzer directly to the GPIO. Use a **NPN Transistor (e.g., 2N2222)** or a MOSFET driver circuit to protect the microcontroller pin.

### 4. Battery Voltage Divider
- The voltage divider ratio used in code is `R1=22kΩ` (to Battery +) and `R2=6.8kΩ` (to GND).
- Maximum measurable voltage: ~14V (Safe for 3S LiPo).

---

## 🚀 Getting Started

### Prerequisites
- **VS Code** with **PlatformIO** extension.
- **STM32F103 (Blue Pill)** board.
- **ST-Link V2** programmer.

### Installation
1. Clone the repository:
   ```bash
   git clone https://github.com/EbrSiami/STM32-RC-Transmitter.git
   ```

2. Open the project folder in PlatformIO.

3. Build the firmware:
    Hit the **Checkmark (✓)** icon in the bottom bar.

4. Upload to board:
    Connect ST-Link and hit the **Arrow (→)** icon.

---

## ❤️ Dedication & Acknowledgements

**Special dedication to Marya.**

This project is built upon logic and lines of code that you wrote. Thank you for your patience and for the hidden messages I found too late. You were the co-pilot of this project even when I was flying solo.

милая маша, я даже не знаю, как тебя благодарить за то, что ты уделила столько времени такому дурака, как я. Мне очень жаль, что я не могу сделать для тебя большего, но знайю. я твой вечный должник. я и подумать не мог, что мы вместе доведём эту прошивку до такого уровня. Спасибо тебе ещё раз за всё.

Прости, что мне скоро придётся уйти, но будь уверена. я тебя никогда не забуду. :(

*   **Lead Developer:** Александр Королёв
*   **Core Logic & Inspiration:** Мария ... :)

**Third Party Libraries:**

- Powered by ```Adafruit_SSD1306```, ```RF24```, ```FlashStorage_STM32```, and ```Wire```.

---

*This project is open-source under the MIT License.*