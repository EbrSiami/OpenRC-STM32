/**
 * @file main.cpp
 * @author Ebrahim Siami
 * @brief STM32 RC Transmitter Firmware - Main Entry Point
 * @version 4.0.1
 * @date 2026-04-25
 * @copyright Copyright (c) 2026
 * Dedicated to Marya for the core logic contributions.
 * 
 * Target Board: STM32F103C8 (Blue Pill)
 * Framework: Arduino via PlatformIO
 */

#include <SPI.h>
#include <Wire.h>
#include <FlashStorage_STM32.h>
#include "DisplayManager.h"
#include "sim_protocol.h" // my own little library to send data
#include "Settings.h"
#include "Buzzer.h"
#include "Button.h"
#include "Radio.h"

// =============================================================================
// --- Hardware Configuration & Pin Definitions ---
// =============================================================================

// Trim Buttons (Active Low)
#define TRIM_BTN_1 PB15
#define TRIM_BTN_2 PA8
#define TRIM_BTN_3 PA9
#define TRIM_BTN_4 PA10
#define TRIM_BTN_5 PA15
#define TRIM_BTN_6 PB3

// Navigation Buttons
#define BTN_ENTER  PB12
#define BTN_UP     PB13
#define BTN_DOWN   PB14

// Peripherals
#define BUZZER_PIN PC13
const int VOLTAGE_PIN = PA4;

#define SETTINGS_MAGIC 0x2C4A1DF2

// =============================================================================
// --- Global Objects & Variables ---
// =============================================================================

extern RadioSettings settings;

// --- Button Instances ---
// Debounce times: 100ms for nav, 50ms for trims
Button enterButton(BTN_ENTER, 100);
Button upButton(BTN_UP, 100);
Button downButton(BTN_DOWN, 100);

Button trimButton1(TRIM_BTN_1, 50);
Button trimButton2(TRIM_BTN_2, 50);
Button trimButton3(TRIM_BTN_3, 50);
Button trimButton4(TRIM_BTN_4, 50);
Button trimButton5(TRIM_BTN_5, 50);
Button trimButton6(TRIM_BTN_6, 50);

// --- UI & Menu State ---
DisplayState currentPage = PAGE_MAIN3;
int trimsMenuIndex = 0;
int settingsMenuIndex = 0;
int featuresMenuIndex = 0;
int invertMenuIndex = 0;
int drMenuIndex = 0;
int advChannelSelectIndex = 0;
int advConfigMenuIndex = 0;
int currentEditingChannel = 0;
bool isAdvEditMode = false;
bool isDREditMode = false;

// --- Auto-return to PAGE_MAIN3 Configuration ---
const unsigned long AUTO_RETURN_TIMEOUT_MS = 7000; // 7 seconds
unsigned long lastNavigationButtonTime = 0; // Track last button press

// --- Trim Configuration ---
const int TRIM_STEP = 10;
const int MIN_TRIM_VALUE = 1024;
const int MAX_TRIM_VALUE = 3072;

// --- Battery Monitor Configuration ---
const float R1 = 22.0;
const float R2 = 6.8;
const float ADC_MAX_VOLTAGE = 3.3;
const float CORRECTION_FACTOR = 1.035;
const float LOW_BATT_WARNING_VOLTAGE = 7.4;

// i'll put this right here to make loop a bit lighter
const float VOLTAGE_CONVERSION_FACTOR = (ADC_MAX_VOLTAGE / 4095.0) * ((R1 + R2) / R2) * CORRECTION_FACTOR;

unsigned long lastBatteryReadTime = 0;
const unsigned long BATTERY_INTERVAL = 250; // read fucking battery voltage every 250ms (4 times a second)
const float BATTERY_ALPHA = 0.1; // filter factor (lower value, better filter but slower)

float batteryVoltage = 0.0;
bool lowBatteryWarningActive = false;

// --- Low Battery Alarm Variables ---
unsigned long lastLowBattBeepMs = 0;
const unsigned long LOW_BATT_REPEAT_MS = 3000;

// --- Timer System ---
unsigned long timerStartMillis = 0;
unsigned long countdownStartMillis = 0;
unsigned long lastTimerUpdateMillis = 0;
int selectedTimerMinutes = -1;
bool isTimerArmed = false;
bool isTimerRunning = false;
long timerRemainingMillis = 0;
bool isTimeEditMode = false;

// --- Radio & Telemetry ---
RadioSettings settings;
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 2; // ~500Hz Update Rate
data_t data;

// --- Display Refresh Logic ---
unsigned long lastDisplayTime = 0;

// --- EMA Filter ---
// K value : higher value is a softer filter but higher latency 
// K=1 50% new data, 50% old data
// K=2 25% new data, 75% old data
const int FILTER_SHIFT = 1; 

// --- Analog Filter State ---
int filteredChannels[6] = {2048, 2048, 2048, 2048, 2048, 2048};

// --- Calibration ---
uint8_t calibStep = 0; // 0: Intro, 1: Center Sticks, 2: Move Sticks, 3: Done
int tempCalibMin[4];
int tempCalibMax[4];

// EXPO Menu States
int expoMenuIndex = 0;
bool isExpoEditMode = false;

// center deadband
const int deadband = 50;  // NOTE: it depends on the quality of sticks youre using.

// ADC update rate
unsigned long lastAdcTime = 0;
const unsigned long ADC_INTERVAL = 2;

// i know everything is Fucking israel fault, fuck zionist forever, fuck jews forever. FUCK they all.

// --- Simulator Mode Configuration ---
// false = Dont send simulator data from USB, keep sending packets to radio
// true  = Stop sending data packets to radio and send simulator data via USB
bool simulatorMode;     // by the way im not going to save it in EEPROM for safety reasons.

// =============================================================================
// --- Helper Functions ---
// =============================================================================

/**
 * @brief Resets the transmission data structure to default safe values.
 */
void ResetData() {
    data.throttle = 0;
    data.pitch    = 1024; // Center
    data.roll     = 1024; // Center
    data.yaw      = 1024; // Center
    data.aux1     = 128;
    data.aux2     = 128;
    data.aux3     = false;
    data.aux4     = false;
}

/**
 * @brief a fast and light EMA filter without float :)
 */
int applyAnalogFilter(int rawValue, int channelIndex) {
    int diff = rawValue - filteredChannels[channelIndex];

    if (diff > 0) diff += (1 << (FILTER_SHIFT - 1));
    else if (diff < 0) diff -= (1 << (FILTER_SHIFT - 1));
    
    filteredChannels[channelIndex] += (diff >> FILTER_SHIFT);
    return filteredChannels[channelIndex];
}

void updateBatteryMonitor() {
    if (millis() - lastBatteryReadTime >= BATTERY_INTERVAL) {
        lastBatteryReadTime = millis();

        int adcValue = analogRead(VOLTAGE_PIN);
        
        if (adcValue > 100) {
            // 1. read the raw voltage value
            float rawVoltage = adcValue * VOLTAGE_CONVERSION_FACTOR;

            // 2. apply the filter
            if (batteryVoltage == 0.0) {
                 // for the system starts
                batteryVoltage = rawVoltage;
            } else {
                 // EMA filter furmula
                batteryVoltage = (BATTERY_ALPHA * rawVoltage) + ((1.0 - BATTERY_ALPHA) * batteryVoltage);
            }
        } else {
            // if the battery disconnected or voltage was incorrect
            batteryVoltage = 0.0; 
        }
    }
}

/**
 * @brief Manages the non-blocking state machine for the low battery alarm.
 */
void handleLowBatteryAlarm() {
    bool low = (batteryVoltage < LOW_BATT_WARNING_VOLTAGE && batteryVoltage > 4.0);

    if (!low) {
        lowBatteryWarningActive = false;
        return;
    }

    lowBatteryWarningActive = true;

    unsigned long now = millis();
    if (now - lastLowBattBeepMs >= LOW_BATT_REPEAT_MS) {
        playBeepEvent(EVT_LOW_BATTERY);
        lastLowBattBeepMs = now;
    }
}

uint8_t calculateChecksum() {
    uint8_t* data = (uint8_t*)&settings;
    uint8_t sum = 0;
    for (size_t i = 0; i < offsetof(RadioSettings, checksum); i++) {
        sum ^= data[i];
    }
    return sum;
}

void saveSettings() {
    settings.checksum = calculateChecksum();
    noInterrupts();
    EEPROM.put(10, settings);
    interrupts();
}

void loadSettings() {

    EEPROM.get(10, settings);

    uint8_t calcChecksum = calculateChecksum();

    bool invalid = (settings.magic != SETTINGS_MAGIC) || (settings.checksum != calculateChecksum());

    if (invalid) {

        memset(&settings, 0, sizeof(RadioSettings));

        settings.magic = SETTINGS_MAGIC;

        settings.trim1 = 2048;
        settings.trim2 = 2048;
        settings.trim3 = 2048;
        settings.airplaneMode = false;
        settings.buzzerEnabled = true;
        settings.lightModeEnabled = false;

        // Be sure about default D/R values
        settings.dualRateRoll = 100;
        settings.dualRatePitch = 100;
        settings.dualRateYaw = 100;
        settings.dualRateEnabled = false;

        // Checking the calibration
        for (int i = 0; i < 4; i++) {
            settings.calibMin[i] = 0;
            settings.calibCenter[i] = 2048;
            settings.calibMax[i] = 4095;
        }

        // Checking the Expo values
        settings.expoRoll = 0;
        settings.expoPitch = 0;
        settings.expoYaw = 0;

        // Default Channels mix
        settings.mixMode = 0;

        // check the invert channl status
        for (int i = 0; i < 8; i++) {
            settings.channelInverted[i] = false;
        }

        for (int i = 0; i < 4; i++) {
            settings.epaMin[i] = 0;
            settings.subTrim[i] = 2048;
            settings.epaMax[i] = 4095;
        }

        // finally saves the settings 
        saveSettings();
    }
}

bool isThrottleActive(uint16_t thr) {
    if (settings.airplaneMode) return (thr > 12); // Above 5%
    return (thr < 900 || thr > 1200);             // Outside center deadband
}

/**
 * @brief Handles the countdown logic and triggers alarm when time is up.
 */
void handleTimerLogic(uint16_t currentThrottle) {
    unsigned long currentMillis = millis();
    unsigned long delta = currentMillis - lastTimerUpdateMillis;
    lastTimerUpdateMillis = currentMillis;

    if (!isTimerArmed) {
        isTimerRunning = false;
        return; 
    }

    // Check if throttle is active to "Run" the clock
    isTimerRunning = isThrottleActive(currentThrottle);

    if (isTimerRunning) {
        long oldTime = timerRemainingMillis;

        if (selectedTimerMinutes == 0) {
            // the Stopwatch mode (count-up timer)
            timerRemainingMillis += delta;
        } 
        else {
            timerRemainingMillis -= delta;

            long currentSec = timerRemainingMillis / 1000;
            long oldSec = oldTime / 1000;

            if (currentSec != oldSec && currentSec >= 0) {
                
                // 1- 1 min left
                if (currentSec == 60) {
                    playBeepEvent(EVT_TIMER_1MIN); // 2 beeps
                }
                // 2- 30 seconds left
                else if (currentSec == 30) {
                    playBeepEvent(EVT_TIMER_30SEC); // a shorter one beep
                }
                // 3- last 10 seconds! 
                else if (currentSec <= 10 && currentSec > 0) {
                    playBeepEvent(EVT_TIMER_TICK); // a short beep every second
                }
            }
            // ------------------------------------------

            // if it reached zerp
            if (oldTime > 0 && timerRemainingMillis <= 0) {
                playBeepEvent(EVT_TIMER_DONE); // end timer long beep
            } 
            // count down timer
            else if (timerRemainingMillis <= 0) {
                long oldOverdueInterval = (-oldTime) / 5000;
                long newOverdueInterval = (-timerRemainingMillis) / 5000;

                // alert the use every 5 sencod that timer finished!
                if (newOverdueInterval > oldOverdueInterval) {
                    playBeepEvent(EVT_ERROR); 
                }
            }
        }
    }
}

void scrollMenu(int &currentIndex, int maxIndex, bool scrollDown) {
    if (scrollDown) {
        currentIndex = (currentIndex + 1) % (maxIndex + 1);
    } else {
        currentIndex = (currentIndex - 1 + (maxIndex + 1)) % (maxIndex + 1);
    }
}

/**
 * @brief Resets the auto-return timeout timer.
 * Call this whenever a navigation button is pressed.
 */
void resetAutoReturnTimer() {
    lastNavigationButtonTime = millis();
}

/**
 * @brief Checks if we should auto-return to PAGE_MAIN3 after inactivity.
 * Call this in the main loop.
 */
void checkAutoReturn() {
    unsigned long currentTime = millis();
    bool timedOut = (currentTime - lastNavigationButtonTime >= AUTO_RETURN_TIMEOUT_MS);
    
    if (!timedOut) return;
    
    // ============================================================
    // --- DO NOT AUTO-RETURN if in these conditions ---
    // ============================================================
    
    // 1. Already on PAGE_MAIN3 - no point returning to itself
    if (currentPage == PAGE_MAIN3) {
        resetAutoReturnTimer(); // Keep resetting timer while on main page
        return;
    }
    
    // 2. In any edit mode - user is actively configuring something
    if (isTimeEditMode || isDREditMode || isExpoEditMode || isAdvEditMode) {
        resetAutoReturnTimer(); // Don't timeout while editing
        return;
    }
    
    // 3. In calibration menu - user is working with sticks
    if (currentPage == PAGE_CALIBRATION) {
        resetAutoReturnTimer(); // Don't timeout during calibration
        return;
    }
    
    // Reset all edit mode flags before returning
    isTimeEditMode = false;
    isDREditMode = false;
    isExpoEditMode = false;
    isAdvEditMode = false;
    
    // Return to main page
    currentPage = PAGE_MAIN3;
    settingsMenuIndex = 0;
    
    // User feedback
    playBeepEvent(EVT_CONFIRM);
    resetAutoReturnTimer(); // Reset timer after returning
}

/**
 * @brief Main State Machine for UI Navigation.
 * Handles button presses for Up, Down, and Enter across different pages.
 */
void handleNavigationButtons() {
    int currentMaxIndex = 0;
    int* activeIndexPtr = &settingsMenuIndex;

    // 1. set the maximum items and active index based on current page
    switch (currentPage) {
        case PAGE_MAIN3:      currentMaxIndex = 3; activeIndexPtr = &settingsMenuIndex; break;
        case PAGE_MAIN1:
        case PAGE_MAIN2:      currentMaxIndex = 1; activeIndexPtr = &settingsMenuIndex; break;
        case PAGE_TRIMS:      currentMaxIndex = 2; activeIndexPtr = &trimsMenuIndex; break;
        case MENU:            currentMaxIndex = SETTING_TOTAL - 1; activeIndexPtr = &settingsMenuIndex; break;
        case PAGE_CH_INVERT:  currentMaxIndex = 8; activeIndexPtr = &invertMenuIndex; break;
        case PAGE_FEATURES:   currentMaxIndex = FEATURE_BACK; activeIndexPtr = &featuresMenuIndex; break;
        case PAGE_DUAL_RATE:  currentMaxIndex = 5; activeIndexPtr = &drMenuIndex; break;
        case PAGE_CHANNELS_ADVANCED: currentMaxIndex = 4; activeIndexPtr = &advChannelSelectIndex; break;
        case PAGE_CHANNEL_CONFIG:    currentMaxIndex = 4; activeIndexPtr = &advConfigMenuIndex; break;
        case PAGE_EXPO: currentMaxIndex = 4; activeIndexPtr = &expoMenuIndex; break;
    }

    // ----------------------
    // --- UP BUTTON Logic ---
    // ----------------------
    if (upButton.wasJustPressed() || upButton.isAutoRepeating(500, 150)) {
        playBeepEvent(EVT_NAV); // Now it beeps rapidly while holding!
        resetAutoReturnTimer(); // Reset timeout when button is pressed
        
        if (currentPage == PAGE_CALIBRATION && calibStep == 0) {
            currentPage = PAGE_FEATURES;
            featuresMenuIndex = FEATURE_CALIBRATION;
            playBeepEvent(EVT_CANCEL);
        }
        else if (isTimeEditMode) {
            selectedTimerMinutes++;
            if (selectedTimerMinutes > 60) {
                selectedTimerMinutes = -1; // Wrap around to -1
            }
        }
        else if (currentPage == PAGE_DUAL_RATE && isDREditMode) {
            if (drMenuIndex == 2 && settings.dualRateRoll < 100) settings.dualRateRoll += 5;
            if (drMenuIndex == 3 && settings.dualRatePitch < 100) settings.dualRatePitch += 5;
            if (drMenuIndex == 4 && settings.dualRateYaw < 100) settings.dualRateYaw += 5;
        }
        else if (currentPage == PAGE_EXPO && isExpoEditMode) {
            if (expoMenuIndex == 1 && settings.expoRoll < 100) settings.expoRoll += 5;
            if (expoMenuIndex == 2 && settings.expoPitch < 100) settings.expoPitch += 5;
            if (expoMenuIndex == 3 && settings.expoYaw < 100) settings.expoYaw += 5;
        }
        else if (currentPage == PAGE_CHANNEL_CONFIG && isAdvEditMode) {
            if (advConfigMenuIndex == 1 && settings.epaMin[currentEditingChannel] < 2000) settings.epaMin[currentEditingChannel] += 10;
            if (advConfigMenuIndex == 2 && settings.subTrim[currentEditingChannel] < 4095) settings.subTrim[currentEditingChannel] += 10;
            if (advConfigMenuIndex == 3 && settings.epaMax[currentEditingChannel] < 4095) settings.epaMax[currentEditingChannel] += 10;
        }
        else if (currentPage == MENU) {
            settingsMenuIndex--;
            if (settingsMenuIndex < 0) settingsMenuIndex = SETTING_NEXT;
        } 
        else {
            scrollMenu(*activeIndexPtr, currentMaxIndex, false);
        }
    }

    // ------------------------
    // --- DOWN BUTTON Logic ---
    // ------------------------
    if (downButton.wasJustPressed() || downButton.isAutoRepeating(500, 150)) {
        playBeepEvent(EVT_NAV);
        resetAutoReturnTimer(); // Reset timeout when button is pressed
        
        if (currentPage == PAGE_CALIBRATION && calibStep == 0) {
            currentPage = PAGE_FEATURES;
            featuresMenuIndex = FEATURE_CALIBRATION;
            playBeepEvent(EVT_CANCEL);
        }
        else if (isTimeEditMode) {
            selectedTimerMinutes--;
            if (selectedTimerMinutes < -1) {
                selectedTimerMinutes = 60; // Wrap around to 60
            }
        }
        else if (currentPage == PAGE_DUAL_RATE && isDREditMode) {
            if (drMenuIndex == 2 && settings.dualRateRoll > 10) settings.dualRateRoll -= 5;
            if (drMenuIndex == 3 && settings.dualRatePitch > 10) settings.dualRatePitch -= 5;
            if (drMenuIndex == 4 && settings.dualRateYaw > 10) settings.dualRateYaw -= 5;
        }
        else if (currentPage == PAGE_EXPO && isExpoEditMode) {
            if (expoMenuIndex == 1 && settings.expoRoll > -100) settings.expoRoll -= 5;
            if (expoMenuIndex == 2 && settings.expoPitch > -100) settings.expoPitch -= 5;
            if (expoMenuIndex == 3 && settings.expoYaw > -100) settings.expoYaw -= 5;
        }
        else if (currentPage == PAGE_CHANNEL_CONFIG && isAdvEditMode) {
            if (advConfigMenuIndex == 1 && settings.epaMin[currentEditingChannel] > 0) settings.epaMin[currentEditingChannel] -= 10;
            if (advConfigMenuIndex == 2 && settings.subTrim[currentEditingChannel] > 0) settings.subTrim[currentEditingChannel] -= 10;
            if (advConfigMenuIndex == 3 && settings.epaMax[currentEditingChannel] > 2000) settings.epaMax[currentEditingChannel] -= 10;
        }
        else if (currentPage == MENU) {
            settingsMenuIndex++;
            if (settingsMenuIndex > SETTING_NEXT) settingsMenuIndex = 0;
        } 
        else {
            scrollMenu(*activeIndexPtr, currentMaxIndex, true);
        }
    }

    // -------------------------
    // --- ENTER BUTTON Logic ---
    // -------------------------
    if (enterButton.wasJustPressed()) {
        resetAutoReturnTimer(); // Reset timeout when button is pressed

        switch (currentPage) {
            
            case PAGE_MAIN3:
                if (settingsMenuIndex == 2) {
                    if (isTimeEditMode) {
                        // 1. exit the edit mode
                        isTimeEditMode = false; 
                        
                        // 2. applying the timer settings
                        if (selectedTimerMinutes == -1) {
                            isTimerArmed = false; // turning off timer
                        } else if (selectedTimerMinutes == 0) {
                            timerRemainingMillis = 0;       // stopwatch starts from zero
                            isTimerArmed = true;
                        } else {
                            timerRemainingMillis = (long)selectedTimerMinutes * 60 * 1000;
                            isTimerArmed = true;
                        }
                    } else {
                        // enter the edit mode
                        isTimeEditMode = true;
                        isTimerArmed = false;
                        isTimerRunning = false;
                    }
                    playBeepEvent(EVT_CONFIRM);
                }
                else if (settingsMenuIndex == 3) {
                    settings.dualRateEnabled = !settings.dualRateEnabled;
                    saveSettings();
                    playBeepEvent(EVT_CLICK);
                }
                else if (settingsMenuIndex == 0) { currentPage = PAGE_MAIN1; settingsMenuIndex = 0; playBeepEvent(EVT_CLICK); }
                else if (settingsMenuIndex == 1) { currentPage = MENU; settingsMenuIndex = SETTING_NEXT; playBeepEvent(EVT_CLICK); }
                break;

            case PAGE_MAIN1:
                if (settingsMenuIndex == 0) { currentPage = PAGE_MAIN2; settingsMenuIndex = 0; }
                else if (settingsMenuIndex == 1) { currentPage = PAGE_MAIN3; settingsMenuIndex = 0; }
                playBeepEvent(EVT_CLICK);
                break;

            case PAGE_MAIN2:
                if (settingsMenuIndex == 0) { currentPage = PAGE_TRIMS; trimsMenuIndex = 1; }
                else if (settingsMenuIndex == 1) { currentPage = PAGE_MAIN1; settingsMenuIndex = 1; }
                playBeepEvent(EVT_CLICK);
                break;

            case PAGE_TRIMS:
                if (trimsMenuIndex == 0) { 
                    saveSettings(); 
                    showSavingFeedback(); 
                    playBeepEvent(EVT_CONFIRM);
                }
                else if (trimsMenuIndex == 1) { currentPage = MENU; settingsMenuIndex = SETTING_NEXT; playBeepEvent(EVT_CLICK); }
                else if (trimsMenuIndex == 2) { currentPage = PAGE_MAIN2; settingsMenuIndex = 1; playBeepEvent(EVT_CANCEL); }
                break;

            case MENU:
                switch (settingsMenuIndex) {
                    case SETTING_BACK:
                        currentPage = PAGE_TRIMS; trimsMenuIndex = 2; playBeepEvent(EVT_CANCEL); break;
                    case SETTING_LIGHT_MODE:
                        settings.lightModeEnabled = !settings.lightModeEnabled; saveSettings(); showSavingFeedback(); playBeepEvent(EVT_CONFIRM); break;
                    case SETTING_BUZZER:
                        settings.buzzerEnabled = !settings.buzzerEnabled; saveSettings(); showSavingFeedback(); playBeepEvent(EVT_CONFIRM); break;
                    case SETTING_THROTTLE_MODE:
                        settings.airplaneMode = !settings.airplaneMode; saveSettings(); showSavingFeedback(); playBeepEvent(EVT_CONFIRM); break;
                    case SETTING_RESET_TRIMS:
                        settings.trim1 = 2048; settings.trim2 = 2048; settings.trim3 = 2048; saveSettings(); showSavingFeedback(); playBeepEvent(EVT_CONFIRM); break;
                    case SETTING_CH_INVERT:
                        currentPage = PAGE_CH_INVERT; invertMenuIndex = 0; playBeepEvent(EVT_CLICK); break;
                    case SETTING_INFO:
                        currentPage = PAGE_INFO; playBeepEvent(EVT_CLICK); break;
                    case SETTING_NEXT:
                        currentPage = PAGE_FEATURES; featuresMenuIndex = FEATURE_BACK; playBeepEvent(EVT_CLICK); break;
                }
                break;

            case PAGE_FEATURES:
                switch (featuresMenuIndex) {
                    case FEATURE_BACK:
                        currentPage = MENU; settingsMenuIndex = SETTING_BACK; playBeepEvent(EVT_CANCEL); break;
                    case FEATURE_DUAL_RATE:
                        currentPage = PAGE_DUAL_RATE; drMenuIndex = 0; playBeepEvent(EVT_CLICK); break; 
                    case FEATURE_CHANNEL_ADVANCED:
                        currentPage = PAGE_CHANNELS_ADVANCED; advChannelSelectIndex = 0; playBeepEvent(EVT_CLICK); break;
                    case FEATURE_EXPO:
                        currentPage = PAGE_EXPO; 
                        expoMenuIndex = 0; 
                        playBeepEvent(EVT_CLICK); 
                        break;
                    case FEATURE_CALIBRATION:
                        currentPage = PAGE_CALIBRATION; playBeepEvent(EVT_CLICK); break;
                    case FEATURE_CHANNELS_MIX:
                        settings.mixMode = (settings.mixMode + 1) % 5; // very genius way i learned today!
                        saveSettings();
                        showSavingFeedback();
                        playBeepEvent(EVT_CONFIRM);
                        break;
                    case FEATURE_SIMULATOR:
                        simulatorMode = !simulatorMode;

                        if (simulatorMode) {
                            setRadioPower(false);
                        } else {
                            setRadioPower(true);
                        }

                        playBeepEvent(EVT_CONFIRM);
                        break;
                }
                break;

            case PAGE_DUAL_RATE:
                if (drMenuIndex >= 2 && drMenuIndex <= 4) {
                    isDREditMode = !isDREditMode;
                    playBeepEvent(EVT_CONFIRM);
                }
                else if (drMenuIndex == 5) {
                    saveSettings();
                    showSavingFeedback();
                    playBeepEvent(EVT_CONFIRM);
                }
                else if (drMenuIndex == 0) { 
                    currentPage = PAGE_FEATURES;
                    drMenuIndex = 0; 
                    playBeepEvent(EVT_CLICK); 
                }
                else if (drMenuIndex == 1) {
                    playBeepEvent(EVT_CLICK);
                }
                break;

            case PAGE_CH_INVERT:
                if (invertMenuIndex == 8) { 
                    currentPage = MENU; settingsMenuIndex = SETTING_CH_INVERT;
                    playBeepEvent(EVT_CANCEL); showSavingFeedback();            // Exit the MENU
                } else {
                    settings.channelInverted[invertMenuIndex] = !settings.channelInverted[invertMenuIndex];
                    saveSettings();
                    playBeepEvent(EVT_CONFIRM);
                }
                break;

            case PAGE_INFO:
                currentPage = MENU; playBeepEvent(EVT_CANCEL); break; // Exit the info page 
                // fuck israel and trump btw

            case PAGE_CALIBRATION:
                if (calibStep == 0) {
                    // setting the center
                    calibStep = 1;
                    playBeepEvent(EVT_CLICK);
                } 
                else if (calibStep == 1) {
                    // save the sticks center
                    settings.calibCenter[0] = applyAnalogFilter(analogRead(PA0), 0);
                    settings.calibCenter[1] = applyAnalogFilter(analogRead(PA1), 1);
                    settings.calibCenter[2] = applyAnalogFilter(analogRead(PA2), 2);
                    settings.calibCenter[3] = applyAnalogFilter(analogRead(PA3), 3);
                    
                    // get ready for the next step
                    for(int i=0; i<4; i++) {
                        tempCalibMin[i] = settings.calibCenter[i];
                        tempCalibMax[i] = settings.calibCenter[i];
                    }
                    calibStep = 2;
                    playBeepEvent(EVT_CONFIRM);
                }
                else if (calibStep == 2) {
                    bool isValid = true;
                    int minRange = 1000;
                    
                    for(int i=0; i<4; i++) {
                        if ((tempCalibMax[i] - tempCalibMin[i]) < minRange) {
                            isValid = false;
                            break;
                        }
                    }

                    if (isValid) {
                        for(int i=0; i<4; i++) {
                            settings.calibMin[i] = tempCalibMin[i];
                            settings.calibMax[i] = tempCalibMax[i];
                        }
                        saveSettings();
                        showSavingFeedback(); 
                        calibStep = 3;
                        playBeepEvent(EVT_CONFIRM);
                    } else {
                        calibStep = 0;
                        currentPage = PAGE_FEATURES;
                        playBeepEvent(EVT_ERROR);
                    }
                }
                else if (calibStep == 3) {
                    // exit the calibration menu
                    calibStep = 0;
                    currentPage = PAGE_FEATURES; 
                    playBeepEvent(EVT_CANCEL); 
                }
                break;

            case PAGE_CHANNELS_ADVANCED:
                if (advChannelSelectIndex == 4) {
                    currentPage = PAGE_FEATURES;
                    featuresMenuIndex = FEATURE_CHANNEL_ADVANCED;
                    playBeepEvent(EVT_CANCEL);
                } else {
                    currentEditingChannel = advChannelSelectIndex;
                    currentPage = PAGE_CHANNEL_CONFIG;
                    advConfigMenuIndex = 0;
                    isAdvEditMode = false;
                    playBeepEvent(EVT_CLICK);
                }
                break;

            case PAGE_CHANNEL_CONFIG:
                if (advConfigMenuIndex == 0) {
                    currentPage = PAGE_CHANNELS_ADVANCED;
                    playBeepEvent(EVT_CANCEL);
                } 
                else if (advConfigMenuIndex == 4) {
                    saveSettings();
                    showSavingFeedback();
                    currentPage = PAGE_CHANNELS_ADVANCED;
                    playBeepEvent(EVT_CONFIRM);
                } 
                else {
                    isAdvEditMode = !isAdvEditMode;
                    if (!isAdvEditMode) playBeepEvent(EVT_CONFIRM);
                    else playBeepEvent(EVT_CLICK);
                }
                break;

            case PAGE_EXPO:
                if (expoMenuIndex >= 1 && expoMenuIndex <= 3) {
                    isExpoEditMode = !isExpoEditMode;
                    playBeepEvent(EVT_CONFIRM);
                }
                else if (expoMenuIndex == 4) {
                    saveSettings();
                    showSavingFeedback();
                    currentPage = PAGE_FEATURES;
                    playBeepEvent(EVT_CONFIRM);
                }
                else if (expoMenuIndex == 0) { 
                    currentPage = PAGE_FEATURES;
                    featuresMenuIndex = FEATURE_EXPO;
                    playBeepEvent(EVT_CANCEL); 
                }
                break;
        }
    }
}

void applyTrimStep(int &trimValue, bool isUp, bool allowBeep) {
    int oldValue = trimValue;

    if (isUp) {
        // If moving UP and we are just below center, snap exactly to center
        if (trimValue < 2048 && trimValue + TRIM_STEP >= 2048) {
            trimValue = 2048;
        } else if (trimValue < MAX_TRIM_VALUE) {
            trimValue += TRIM_STEP;
        }
    } else {
        // If moving DOWN and we are just above center, snap exactly to center
        if (trimValue > 2048 && trimValue - TRIM_STEP <= 2048) {
            trimValue = 2048;
        } else if (trimValue > MIN_TRIM_VALUE) {
            trimValue -= TRIM_STEP;
        }
    }

    trimValue = constrain(trimValue, MIN_TRIM_VALUE, MAX_TRIM_VALUE);

    if (trimValue == oldValue) {
        if (allowBeep) playBeepEvent(EVT_TRIM_LIMIT);
        return;
    }

    if (!allowBeep) return;

    if (trimValue == 2048) {
        playBeepEvent(EVT_TRIM_CENTER);
    } else if (trimValue == MAX_TRIM_VALUE || trimValue == MIN_TRIM_VALUE) {
        playBeepEvent(EVT_TRIM_LIMIT);
    } else {
        playBeepEvent(EVT_TRIM_STEP);
    }
}

/**
 * @brief Handles the 6 Trim buttons (3 sets of +/-).
 * Includes hold-to-repeat logic.
 */
void handleTrimButtons() {
    static unsigned long lastTrimTime = 0;
    static unsigned long lastTrimBeepTime = 0;

    const unsigned long TRIM_SPEED_DELAY = 100;
    const unsigned long TRIM_BEEP_DELAY  = 700;

    unsigned long now = millis();
    bool canAdjust = (now - lastTrimTime >= TRIM_SPEED_DELAY);
    bool canBeepOnHold = (now - lastTrimBeepTime >= TRIM_BEEP_DELAY);

    auto processTrim = [&](Button &btn, int &trimValue, bool isUp, int channelIndex) {
        if (!btn.wasJustPressed() && !btn.isBeingHeld()) {
            return;
        }

        bool effectiveIsUp = settings.channelInverted[channelIndex] ? !isUp : isUp;

        if (btn.wasJustPressed()) {
            applyTrimStep(trimValue, effectiveIsUp, true);
            lastTrimTime = now;
            lastTrimBeepTime = now;
            return;
        }

        if (btn.isBeingHeld() && canAdjust) {
            applyTrimStep(trimValue, effectiveIsUp, canBeepOnHold);
            lastTrimTime = now;
            if (canBeepOnHold) lastTrimBeepTime = now;
        }
    };

    processTrim(trimButton1, settings.trim1, true, 0);  // Roll Trim Up
    processTrim(trimButton2, settings.trim1, false, 0); // Roll Trim Down

    processTrim(trimButton3, settings.trim2, true, 1);  // Pitch Trim Up
    processTrim(trimButton4, settings.trim2, false, 1); // Pitch Trim Down

    processTrim(trimButton5, settings.trim3, true, 3);  // Yaw Trim Up
    processTrim(trimButton6, settings.trim3, false, 3); // Yaw Trim Down
}

int processChannel(int rawValue,
                   int calibMin, int calibCenter, int calibMax, int deadband,
                   int expoPercent,
                   int dualRatePercent,
                   int subTrimValue,
                   bool invert,
                   int epaMin, int epaMax)
{

    rawValue = constrain(rawValue, calibMin, calibMax);
    // its really hard to code without you:(

    // --- Step 2: Calibration & Deadband ---
    int val;
    if ((calibCenter - calibMin) < 100 || (calibMax - calibCenter) < 100) {
        return subTrimValue;
    }

    if (abs(rawValue - calibCenter) <= deadband) {
        val = 2048;
    } else if (rawValue < calibCenter) {
        val = map(rawValue, calibMin, calibCenter - deadband, 0, 2048);
    } else {
        val = map(rawValue, calibCenter + deadband, calibMax, 2048, 4095);
    }
    val = constrain(val, 0, 4095);

// --- Step 3: EXPO ---
if (expoPercent != 0) {
    float normalized = (val - 2048) / 2048.0f;
    float expo = expoPercent / 100.0f;
    float output;
    
    if (expo > 0) {
        output = (1.0f - expo) * normalized + expo * (normalized * normalized * normalized);
    } else {
        expo = -expo;
        if (normalized > 0) {
            output = (1.0f - expo) * normalized + expo * (pow(normalized, 1.0/3.0));
        } else {
            output = (1.0f - expo) * normalized + expo * (-pow(-normalized, 1.0/3.0));
        }
    }
    val = 2048 + (int)(output * 2048.0f);
}

    // --- Step 4: Dual Rate (DR) ---
    if (dualRatePercent < 100) {
        long offset = val - 2048;
        offset = (offset * dualRatePercent) / 100;
        val = 2048 + offset;
    }

    // --- Step 5: Reverse ---
    if (invert) {
        val = 4095 - val;
    }

    // --- Step 6: Sub-Trim and EPA (End Point Adjustment) ---
    int result;
    if (val <= 2048) {
        result = map(val, 0, 2048, epaMin, subTrimValue);
    } else {
        result = map(val, 2048, 4095, subTrimValue, epaMax);
    }

    return constrain(result, epaMin, epaMax);
}

// =============================================================================
// --- Main Setup ---
// =============================================================================
void setup() {
    pinMode(VOLTAGE_PIN, INPUT_ANALOG);
    timerStartMillis = millis();

    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);

    pinMode(PB4, INPUT);
    pinMode(PB5, INPUT);

    pinMode(PA0, INPUT_ANALOG); // Roll
    pinMode(PA1, INPUT_ANALOG); // Pitch
    pinMode(PA2, INPUT_ANALOG); // Throttle
    pinMode(PA3, INPUT_ANALOG); // Yaw
    pinMode(PB0, INPUT_ANALOG); // AUX1 Pot
    pinMode(PB1, INPUT_ANALOG); // AUX2 Pot

    // STM32 ADC setup
    analogReadResolution(12);

    // Initialize Buttons
    enterButton.begin(); upButton.begin(); downButton.begin();
    trimButton1.begin(); trimButton2.begin(); trimButton3.begin();
    trimButton4.begin(); trimButton5.begin(); trimButton6.begin();

    // Communications init
    Wire.begin();   // OLED
    Wire.setClock(400000);

    Serial.begin(115200);

    delay(500);

    setupDisplay();
    showSplashScreen("System Init...", 3000);

    buzzer.begin(BUZZER_PIN);

    simulatorMode = false;

    setupRadio();
    loadSettings();

    playBeepEvent(EVT_STARTUP);

    // NOTE : you may need to uncomment these lines for the first upload (you can remove them later).
    //
    // settings.magic = SETTINGS_MAGIC; // honestly im not sure about this one, ask donald trump!
    // settings.trim1 = 2048;
    // settings.trim2 = 2048;
    // settings.trim3 = 2048;
    // settings.airplaneMode = false;
    // settings.buzzerEnabled = true;
    // settings.lightModeEnabled = false;
    // settings.dualRateRoll = 100;
    // settings.dualRatePitch = 100;
    // settings.dualRateYaw = 100;
    // settings.dualRateEnabled = false;
    // for (int i = 0; i < 4; i++) {
    //     settings.calibMin[i] = 0;
    //     settings.calibCenter[i] = 2048;
    //     settings.calibMax[i] = 4095;
    // }
    // settings.expoRoll = 0;
    // settings.expoPitch = 0;
    // settings.expoYaw = 0;
    // settings.mixMode = 0;
    // for (int i = 0; i < 8; i++) {
    //     settings.channelInverted[i] = false;
    // }
    // for (int i = 0; i < 4; i++) {
    //     settings.epaMin[i] = 0;
    //     settings.subTrim[i] = 2048;
    //     settings.epaMax[i] = 4095;
    // }

    ResetData();
    
    // Initialize the auto-return timer
    resetAutoReturnTimer();
}

// =============================================================================
// --- Main Loop ---
// =============================================================================
void loop() {

    unsigned long currentTime = millis();

    unsigned long t1 = millis();

    // 1. Update Input Devices
    enterButton.update(); upButton.update(); downButton.update();
    trimButton1.update(); trimButton2.update(); trimButton3.update();
    trimButton4.update(); trimButton5.update(); trimButton6.update();

    unsigned long t2 = millis();

    buzzer.update(settings.buzzerEnabled);

    unsigned long t3 = millis();

    // 2. Battery Monitoring
    updateBatteryMonitor();

    unsigned long t4 = millis();

    handleLowBatteryAlarm();

    unsigned long t5 = millis();

    // 3. UI Logic Processing
    handleTrimButtons();

    unsigned long t6 = millis();

    handleNavigationButtons();

    unsigned long t7 = millis();

    // 3.5. Check auto-return to PAGE_MAIN3 on inactivity
    checkAutoReturn();

    // 4. Input Mapping (ADC -> Channel Data)

    if (currentTime - lastAdcTime >= ADC_INTERVAL) {
        lastAdcTime = currentTime;

        // a- read the raw value and apply the filter
        int rawRoll     = applyAnalogFilter(analogRead(PA0), 0);
        int rawPitch    = applyAnalogFilter(analogRead(PA1), 1);
        int rawThrottle = applyAnalogFilter(analogRead(PA2), 2);
        int rawYaw      = applyAnalogFilter(analogRead(PA3), 3);
        int rawAux1     = applyAnalogFilter(analogRead(PB0), 4);
        int rawAux2     = applyAnalogFilter(analogRead(PB1), 5);

        // if we are in calibration memu
        if (currentPage == PAGE_CALIBRATION && calibStep == 2) {
            if (rawRoll < tempCalibMin[0]) tempCalibMin[0] = rawRoll;
            if (rawRoll > tempCalibMax[0]) tempCalibMax[0] = rawRoll;
            
            if (rawPitch < tempCalibMin[1]) tempCalibMin[1] = rawPitch;
            if (rawPitch > tempCalibMax[1]) tempCalibMax[1] = rawPitch;
            
            if (rawThrottle < tempCalibMin[2]) tempCalibMin[2] = rawThrottle;
            if (rawThrottle > tempCalibMax[2]) tempCalibMax[2] = rawThrottle;
            
            if (rawYaw < tempCalibMin[3]) tempCalibMin[3] = rawYaw;
            if (rawYaw > tempCalibMax[3]) tempCalibMax[3] = rawYaw;
        }

        // --- Combining Sub-Trim and Digital Trim ---
        int combinedTrimRoll  = settings.subTrim[0] + (settings.trim1 - 2048);
        int combinedTrimPitch = settings.subTrim[1] + (settings.trim2 - 2048);
        int combinedTrimYaw   = settings.subTrim[3] + (settings.trim3 - 2048);

        // --- Process main channels ---
        int roll_12b = processChannel(
            rawRoll,
            settings.calibMin[0], settings.calibCenter[0], settings.calibMax[0], deadband,
            settings.expoRoll,
            settings.dualRateEnabled ? settings.dualRateRoll : 100,
            combinedTrimRoll,
            settings.channelInverted[0],
            settings.epaMin[0], settings.epaMax[0]
        );

        int pitch_12b = processChannel(
            rawPitch,
            settings.calibMin[1], settings.calibCenter[1], settings.calibMax[1], deadband,
            settings.expoPitch,
            settings.dualRateEnabled ? settings.dualRatePitch : 100,
            combinedTrimPitch,
            settings.channelInverted[1],
            settings.epaMin[1], settings.epaMax[1]
        );

        int yaw_12b = processChannel(
            rawYaw,
            settings.calibMin[3], settings.calibCenter[3], settings.calibMax[3], deadband,
            settings.expoYaw,
            settings.dualRateEnabled ? settings.dualRateYaw : 100,
            combinedTrimYaw,
            settings.channelInverted[3],
            settings.epaMin[3], settings.epaMax[3]
        );

        // --- Throttle Logic ---
        int throttle_calibrated;
        if (abs(rawThrottle - settings.calibCenter[2]) <= deadband) {
            throttle_calibrated = 2048;
        } else if (rawThrottle < settings.calibCenter[2]) {
            throttle_calibrated = map(rawThrottle, settings.calibMin[2], settings.calibCenter[2] - deadband, 0, 2048);
        } else {
            throttle_calibrated = map(rawThrottle, settings.calibCenter[2] + deadband, settings.calibMax[2], 2048, 4095);
        }
        
        int throttle_pre_map = throttle_calibrated;
        if (settings.airplaneMode) {
            if (throttle_pre_map < 2048) {
                throttle_pre_map = 0;
            } else {
                throttle_pre_map = map(throttle_pre_map, 2048, 4095, 0, 4095);
            }
        }
        
        // Apply final mapping (Reverse, Subtrim, EPA) for throttle
        if (settings.channelInverted[2]) {
            throttle_pre_map = 4095 - throttle_pre_map;
        }
        int throttle_12b;
        if (throttle_pre_map <= 2048) {
            throttle_12b = map(throttle_pre_map, 0, 2048, settings.epaMin[2], settings.subTrim[2]);
        } else {
            throttle_12b = map(throttle_pre_map, 2048, 4095, settings.subTrim[2], settings.epaMax[2]);
        }
        throttle_12b = constrain(throttle_12b, settings.epaMin[2], settings.epaMax[2]);

        // --- AUX channels and Switches ---
        int aux1_12b = (true ^ settings.channelInverted[4]) ? (4095 - rawAux1) : rawAux1;
        int aux2_12b = (settings.channelInverted[5]) ? (4095 - rawAux2) : rawAux2;

        bool aux3Raw = digitalRead(PB4);
        data.aux3 = settings.channelInverted[6] ? !aux3Raw : aux3Raw;
        bool aux4Raw = digitalRead(PB5);
        data.aux4 = settings.channelInverted[7] ? !aux4Raw : aux4Raw;

        int final_roll_12b  = roll_12b;
        int final_pitch_12b = pitch_12b;
        int final_yaw_12b   = yaw_12b;

        int pitch_offset = pitch_12b - 2048;
        int roll_offset  = roll_12b - 2048;
        int yaw_offset   = yaw_12b - 2048;

        if (settings.mixMode == 1) { 
            // Mode V-Tail A (Default)
            final_pitch_12b = 2048 + (pitch_offset + yaw_offset) / 2;
            final_yaw_12b   = 2048 + (pitch_offset - yaw_offset) / 2;
        } 
        else if (settings.mixMode == 2) { 
            // Mode V-Tail B (inverted)
            final_pitch_12b = 2048 + (pitch_offset - yaw_offset) / 2;
            final_yaw_12b   = 2048 + (pitch_offset + yaw_offset) / 2;
        }
        else if (settings.mixMode == 3) { 
            // Mode Delta A (Default, flying wing)
            final_roll_12b  = 2048 + (pitch_offset + roll_offset) / 2;
            final_pitch_12b = 2048 + (pitch_offset - roll_offset) / 2;
        }
        else if (settings.mixMode == 4) { 
            // Mode Delta B (Inverted)
            final_roll_12b  = 2048 + (pitch_offset - roll_offset) / 2;
            final_pitch_12b = 2048 + (pitch_offset + roll_offset) / 2;
        }

        final_roll_12b  = constrain(final_roll_12b,  settings.epaMin[0], settings.epaMax[0]);
        final_pitch_12b = constrain(final_pitch_12b, settings.epaMin[1], settings.epaMax[1]);
        final_yaw_12b   = constrain(final_yaw_12b,   settings.epaMin[3], settings.epaMax[3]);

        // i think that mix is almost done, hope it works well
        // if fucking jews allows me, fuck israel fuck trump fuck epstein
        // fuck everything in this fucking world

        // i will shift data 1 bit for main channels and 4 bit for Potentiometers
        data.roll     = final_roll_12b >> 1;
        data.pitch    = final_pitch_12b >> 1;
        data.throttle = throttle_12b >> 1;
        data.yaw      = final_yaw_12b >> 1;
        data.aux1     = aux1_12b >> 4;
        data.aux2     = aux2_12b >> 4;
        // finished lets test it

        if (simulatorMode) {
            // fuck everything
            // we will put simulator sending datas here.
            SimProto::send(
                final_roll_12b, 
                final_pitch_12b, 
                throttle_12b, 
                final_yaw_12b, 
                aux1_12b, 
                aux2_12b, 
                data.aux3, 
                data.aux4
            ); 
            // god, pls pls pls help me
            // its 3 am and i still work on this way
            // the fact is that i cant use usb HID in STM32Duino core and iBUS doesnt works well on USB CDC
            // i think that its duo to the packets sizes and timings of simulated serial, so im testing this way.
        }

        handleTimerLogic(data.throttle);
    }

    unsigned long t8 = millis();

    // 5. Radio Transmission
    if (currentTime - lastSendTime >= SEND_INTERVAL) {
        lastSendTime = currentTime;
        if (!simulatorMode && getRadioStatus() == true) {  // send data only when radio is connected and sim is off.
            sendRadioData(data);
        }
    }

    unsigned long t9 = millis();

    // 6. Display Update
    // Dynamic refresh rate based on page to save resources
    // 5fps on main page, 25fps on other pages
    unsigned long dynamicInterval = (currentPage == PAGE_MAIN3) ? 200 : 40;

    if (currentTime - lastDisplayTime >= dynamicInterval) {
        lastDisplayTime = currentTime;

        drawCurrentPage(currentPage, trimsMenuIndex, settingsMenuIndex, featuresMenuIndex,
                    settings, data.throttle, data.pitch, data.roll, data.yaw, data.aux1,
                    data.aux2, data.aux3, data.aux4, batteryVoltage, selectedTimerMinutes,
                    isTimerArmed, isTimerRunning, timerRemainingMillis, isTimeEditMode,
                    invertMenuIndex, drMenuIndex, advChannelSelectIndex, advConfigMenuIndex,
                    currentEditingChannel, isAdvEditMode, expoMenuIndex, isExpoEditMode
                    );
    }

    unsigned long t10 = millis();

//   Serial.print("Update buttons: ");           Serial.println(t2 - t1);
//   Serial.print("Update Buzzer: ");            Serial.println(t3 - t2);
//   Serial.print("Update Battery: ");           Serial.println(t4 - t3);
//   Serial.print("Update Alarm: ");             Serial.println(t5 - t4);
//   Serial.print("Update Trim: ");              Serial.println(t6 - t5);
//   Serial.print("Update NavigationButtons: "); Serial.println(t7 - t6);
//   Serial.print("Update Input Mapping(4): ");  Serial.println(t8 - t7);
//   Serial.print("Update RadioSend: ");         Serial.println(t9 - t8);
//   Serial.print("Update Screen: ");            Serial.println(t10 - t9);
  
//   Serial.println("-------------------------");
//   Serial.print("Whole: ");                    Serial.println(t10 - t1);
//   Serial.println("-------------------------");
}