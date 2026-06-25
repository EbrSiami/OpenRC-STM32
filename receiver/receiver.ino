/**
 * @file    receiver.cpp
 * @project OpenRC-STM32
 * @brief   8-Channel RC Receiver Firmware
 *
 * Receives NRF24L01+ radio packets from the OpenRC transmitter,
 * decodes the custom packed struct, outputs 8-channel PWM via
 * Servo library, and generates a hardware-timed PPM stream on
 * a single pin (TIM3).  A three-state LED indicates radio health
 * and a 500 ms failsafe cuts throttle on link loss.
 *
 * @author  Александр Королёв
 * @version 1.3.2
 * @date    2026-05-13
 * @license MIT
 *
 * @hardware  STM32G030F6P6
 * @framework Arduino (via PlatformIO)
 * @note      Compile with -Os (optimize for size) — this target
 *            has only 32 KB Flash and 8 KB RAM.
 */

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Servo.h>

// ─────────────────────────────────────────────
//  Pin Definitions
// ─────────────────────────────────────────────

/** Onboard LED — used for link-state feedback. */
#define LED_PIN     PC15

/**
 * PPM output pin.
 * Connect to the signal input of a flight-controller or trainer port.
 * Driven by TIM3 via hardware interrupt (see setupPPM / ppmInterruptHandler).
 */
#define PPM_OUT_PIN PC14

// ─────────────────────────────────────────────
//  Radio
// ─────────────────────────────────────────────

/** NRF24L01+  CE → PA4,  CSN → PA5 */
RF24 radio(PA4, PA5);

/** Listening pipe address — must match the transmitter exactly. */
const uint64_t pipeIn = 0xE8E8F0F0E1LL;

// ─────────────────────────────────────────────
//  Receiver State Machine
// ─────────────────────────────────────────────

/**
 * Three operational states reflected on the LED:
 *  - STATE_ERROR        fast blink (100 ms)  — NRF24 not detected at boot
 *  - STATE_DISCONNECTED slow blink (500 ms)  — no packets received
 *  - STATE_CONNECTED    solid ON             — link active
 */
enum ReceiverState {
    STATE_ERROR,
    STATE_DISCONNECTED,
    STATE_CONNECTED
};

ReceiverState currentState = STATE_DISCONNECTED;

// LED blink timing
unsigned long previousLedMillis = 0;
bool ledPinState = false;

// ─────────────────────────────────────────────
//  Radio Packet Format
// ─────────────────────────────────────────────

/**
 * Packed data struct — **must** match the transmitter's definition
 * byte-for-byte.  Total size: 8 bytes.
 *
 * Bit-field layout:
 *   roll / pitch / throttle / yaw  → 11 bits each  (0–2047)
 *   aux1 / aux2                    →  8 bits each  (0–255)
 *   aux3 / aux4                    →  1 bit  each  (0–1)
 *
 * All 11-bit channels map to 1000–2000 µs PWM in the main loop.
 * 8-bit channels also map to 1000–2000 µs.
 * 1-bit channels snap to 1000 or 2000 µs.
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t roll     : 11;
    uint16_t pitch    : 11;
    uint16_t throttle : 11;
    uint16_t yaw      : 11;
    uint8_t  aux1     :  8;
    uint8_t  aux2     :  8;
    uint8_t  aux3     :  1;
    uint8_t  aux4     :  1;
} data_t;
#pragma pack(pop)

data_t data;

// ─────────────────────────────────────────────
//  Servo / PWM Outputs
// ─────────────────────────────────────────────

Servo servos[8];

/**
 * PWM output pins — one per channel, in channel order
 * (Roll, Pitch, Throttle, Yaw, Aux1–Aux4).
 */
uint8_t servoPins[] = {PA0, PA3, PA7, PB0, PA11, PA12, PB3, PB7};

// ─────────────────────────────────────────────
//  PPM Generator  (TIM3, hardware interrupt)
// ─────────────────────────────────────────────

#define PPM_CHANNELS    8

/**
 * Shared buffer between the main loop and the PPM ISR.
 * Written under noInterrupts() / interrupts() guards.
 * Values are in microseconds (1000–2000).
 */
volatile uint16_t ppmValues[PPM_CHANNELS];

/**
 * Total PPM frame period in µs.
 * Standard RC PPM: 22.5 ms frame, each channel slot = channel_µs + 300 µs sync pulse.
 */
#define PPM_FRAME_LENGTH 22500

/** Duration of each sync (low) pulse within a channel slot, in µs. */
#define PPM_SYNC_LENGTH  300

HardwareTimer *ppmTimer;

/** Index of the channel currently being output by the ISR. */
volatile uint8_t currentChannel = 0;

/** ISR toggle flag: true = currently in the HIGH phase of a slot. */
volatile bool ppmState = false;

// ─────────────────────────────────────────────
//  Failsafe
// ─────────────────────────────────────────────

unsigned long lastRecvTime  = 0;
bool failsafeActive         = false;

/**
 * If no valid packet is received within this window (ms),
 * activateFailsafe() is called and the state drops to DISCONNECTED.
 */
const unsigned int FAILSAFE_TIMEOUT = 500;

// ─────────────────────────────────────────────
//  PPM ISR
// ─────────────────────────────────────────────

/**
 * @brief Hardware timer ISR — generates the PPM waveform on PPM_OUT_PIN.
 *
 * Each call toggles the pin and reloads the timer for the next phase:
 *
 *   HIGH phase  →  channel_value - PPM_SYNC_LENGTH  µs
 *   LOW phase   →  PPM_SYNC_LENGTH                  µs  (sync pulse)
 *
 * After all 8 channels, a gap pulse fills the remainder of the
 * 22.5 ms frame before the sequence restarts.
 */
void ppmInterruptHandler() {
    if (ppmState) {
        // End of HIGH phase — pull LOW for sync pulse
        digitalWriteFast(digitalPinToPinName(PPM_OUT_PIN), LOW);
        ppmState = false;

        if (currentChannel < PPM_CHANNELS) {
            // Load the HIGH duration for the next channel
            ppmTimer->setOverflow(ppmValues[currentChannel] - PPM_SYNC_LENGTH, MICROSEC_FORMAT);
            currentChannel++;
        } else {
            // All channels done — compute and output the frame gap
            uint32_t totalChannelTime = 0;
            for (int i = 0; i < PPM_CHANNELS; i++) {
                totalChannelTime += ppmValues[i];
            }
            uint32_t gapTime = PPM_FRAME_LENGTH - totalChannelTime;
            ppmTimer->setOverflow(gapTime - PPM_SYNC_LENGTH, MICROSEC_FORMAT);
            currentChannel = 0;
        }
    } else {
        // End of LOW (sync) phase — pull HIGH to start next channel
        digitalWriteFast(digitalPinToPinName(PPM_OUT_PIN), HIGH);
        ppmState = true;
        ppmTimer->setOverflow(PPM_SYNC_LENGTH, MICROSEC_FORMAT);
    }
}

// ─────────────────────────────────────────────
//  PPM Setup
// ─────────────────────────────────────────────

/**
 * @brief Initialises TIM3 and attaches ppmInterruptHandler.
 *
 * All channels are preset to 1500 µs (neutral), except Throttle
 * (CH3) which starts at 1000 µs (zero) for safety.
 */
void setupPPM() {
    pinMode(PPM_OUT_PIN, OUTPUT);
    digitalWrite(PPM_OUT_PIN, LOW);

    for (int i = 0; i < PPM_CHANNELS; i++) {
        ppmValues[i] = 1500;
    }
    ppmValues[2] = 1000; // Throttle safe-low at startup

    ppmState = false;
    ppmTimer = new HardwareTimer(TIM3);
    ppmTimer->setOverflow(3000, MICROSEC_FORMAT);
    ppmTimer->attachInterrupt(ppmInterruptHandler);
    ppmTimer->resume();
}

// ─────────────────────────────────────────────
//  LED Handler
// ─────────────────────────────────────────────

/**
 * @brief Non-blocking LED state machine — call every loop iteration.
 *
 * STATE_CONNECTED    → solid ON
 * STATE_DISCONNECTED → 500 ms blink
 * STATE_ERROR        → 100 ms rapid blink (NRF24 hardware fault)
 */
void handleLED() {
    unsigned long currentMillis = millis();

    switch (currentState) {
        case STATE_CONNECTED:
            if (!ledPinState) {
                digitalWrite(LED_PIN, HIGH);
                ledPinState = true;
            }
            break;

        case STATE_DISCONNECTED:
            if (currentMillis - previousLedMillis >= 500) {
                previousLedMillis = currentMillis;
                ledPinState = !ledPinState;
                digitalWrite(LED_PIN, ledPinState);
            }
            break;

        case STATE_ERROR:
            if (currentMillis - previousLedMillis >= 100) {
                previousLedMillis = currentMillis;
                ledPinState = !ledPinState;
                digitalWrite(LED_PIN, ledPinState);
            }
            break;
    }
}

// ─────────────────────────────────────────────
//  Failsafe
// ─────────────────────────────────────────────

/**
 * @brief Applies safe neutral / zero values to all outputs.
 *
 * Called when no packet is received within FAILSAFE_TIMEOUT ms.
 * Throttle is forced to 900 µs; all other channels go to 1000 µs.
 * PPM buffer is updated atomically under interrupt lock.
 */
void activateFailsafe() {
    uint16_t failsafe_values[] = {
        1500,  // Roll     — neutral
        1500,  // Pitch    — neutral
         900,  // Throttle — below minimum (motor off)
        1500,  // Yaw      — neutral
        1000,  // Aux1
        1000,  // Aux2
        1000,  // Aux3
        1000   // Aux4
    };

    for (int i = 0; i < PPM_CHANNELS; i++) {
        servos[i].writeMicroseconds(failsafe_values[i]);
    }

    noInterrupts();
    for (int i = 0; i < PPM_CHANNELS; i++) {
        ppmValues[i] = failsafe_values[i];
    }
    interrupts();
}

// ─────────────────────────────────────────────
//  Setup
// ─────────────────────────────────────────────

void setup() {
    pinMode(LED_PIN, OUTPUT);

    // Startup blink — confirms power and GPIO are working
    // (blocking delay is acceptable here; main loop has not started)
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED_PIN, HIGH); delay(100);
        digitalWrite(LED_PIN, LOW);  delay(100);
    }

    // Attach all 8 servo outputs
    for (int i = 0; i < 8; i++) {
        servos[i].attach(servoPins[i]);
    }

    setupPPM();

    SPI.begin();

    if (radio.begin(&SPI) && radio.isChipConnected()) {
        radio.openReadingPipe(1, pipeIn);
        radio.setChannel(100);
        radio.setDataRate(RF24_250KBPS);
        radio.setAutoAck(false);
        radio.setPALevel(RF24_PA_MAX, true);
        radio.startListening();

        lastRecvTime = millis();
        currentState = STATE_DISCONNECTED;
    } else {
        // NRF24 not detected — enter permanent error state
        currentState = STATE_ERROR;
    }
}

// ─────────────────────────────────────────────
//  Main Loop
// ─────────────────────────────────────────────

void loop() {
    handleLED();

    // Nothing else to do if radio hardware is faulty
    if (currentState == STATE_ERROR) return;

    if (radio.available()) {
        radio.read(&data, sizeof(data_t));
        lastRecvTime   = millis();
        failsafeActive = false;
        currentState   = STATE_CONNECTED;

        // Decode packed fields → PWM microseconds
        uint16_t current_values[PPM_CHANNELS];
        current_values[0] = map(data.roll,     0, 2047, 1000, 2000);
        current_values[1] = map(data.pitch,    0, 2047, 1000, 2000);
        current_values[2] = map(data.throttle, 0, 2047, 1000, 2000);
        current_values[3] = map(data.yaw,      0, 2047, 1000, 2000);
        current_values[4] = map(data.aux1,     0,  255, 1000, 2000);
        current_values[5] = map(data.aux2,     0,  255, 1000, 2000);
        current_values[6] = map(data.aux3,     0,    1, 1000, 2000);
        current_values[7] = map(data.aux4,     0,    1, 1000, 2000);

        // Update PWM servo outputs
        for (int i = 0; i < PPM_CHANNELS; i++) {
            servos[i].writeMicroseconds(current_values[i]);
        }

        // Update PPM buffer atomically (read by ISR)
        noInterrupts();
        for (int i = 0; i < PPM_CHANNELS; i++) {
            ppmValues[i] = current_values[i];
        }
        interrupts();

    } else {
        // No packet — check failsafe timeout
        if (!failsafeActive && (millis() - lastRecvTime > FAILSAFE_TIMEOUT)) {
            failsafeActive = true;
            activateFailsafe();
            currentState = STATE_DISCONNECTED;
        }
    }
}