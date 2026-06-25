/**
 * @file DisplayManager.h
 * @author Александр Королёв
 * @brief Interface for OLED Display Management
 * @version 4.0.1
 * @date 2026-04-23
 *
 * Description:
 * Defines display states, menu items, and rendering prototypes.
 * acts as the bridge between the main logic and the visual output.
 */

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Settings.h"

// =============================================================================
// --- Display Configuration ---
// =============================================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

// =============================================================================
// --- Enums & States ---
// =============================================================================

/**
 * @brief Represents the current active screen/page in the UI.
 */
enum DisplayState {
    PAGE_MAIN1,       // Main View: Channels 1-4 (Sticks)
    PAGE_MAIN2,       // Aux View: Channels 5-8 (Switches/Pots)
    PAGE_MAIN3,       // Dashboard: Timer, Battery, System Status
    PAGE_TRIMS,       // Visual Trim Adjustment Page
    MENU,             // General Settings Menu
    PAGE_FEATURES,
    PAGE_INFO,        // About / Credits Screen
    PAGE_CALIBRATION, // Stick Calibration (Placeholder/WIP)
    PAGE_CH_INVERT,    // Dedicated Channel Inversion Menu
    PAGE_DUAL_RATE,
    PAGE_CHANNELS_ADVANCED,
    PAGE_CHANNEL_CONFIG,
    PAGE_EXPO
};

/**
 * @brief Items available in the Settings Menu.
 * NOTE: The order must match the switch-case logic in DisplayManager.cpp
 */
enum SettingsMenu {
    SETTING_LIGHT_MODE,
    SETTING_BUZZER,
    SETTING_CH_INVERT,
    SETTING_RESET_TRIMS,
    SETTING_THROTTLE_MODE, // Switches between Airplane (0-100) and Quad (Center-based)
    SETTING_INFO,
    SETTING_BACK,          // Navigation Back Button
    SETTING_NEXT,
    SETTING_TOTAL          // Sentinel value for loop limits
};

enum FeaturesMenu {
    FEATURE_EXPO,
    FEATURE_DUAL_RATE,
    FEATURE_CHANNEL_ADVANCED,
    FEATURE_CALIBRATION,
    FEATURE_CHANNELS_MIX,
    FEATURE_SIMULATOR,
    FEATURE_BACK,
    FEATURE_TOTAL
};

// =============================================================================
// --- Global Objects & Externs ---
// =============================================================================

// Global Display Object (Defined in DisplayManager.cpp)
extern Adafruit_SSD1306 display;

// Allow display logic to trigger audio feedback (Defined in main.cpp)
extern void beep(int duration_ms = 100, bool force = false);

// =============================================================================
// --- Function Prototypes ---
// =============================================================================

/**
 * @brief Initializes the I2C OLED display and shows the boot logo.
 */
void setupDisplay();

/**
 * @brief Plays the startup animation (MIG-21 Jet).
 * @param productName Text to display during load.
 * @param durationMs Duration of the animation loop.
 */
void showSplashScreen(const char* productName, const unsigned long durationMs);

/**
 * @brief Shows a temporary "Saving..." feedback screen.
 */
void showSavingFeedback();

/**
 * @brief Renders the entire UI frame based on the current state.
 * 
 * @param currentPage The active page enum.
 * @param trimsMenuIndex Cursor position for Trim page.
 * @param settingsMenuIndex Cursor position for Settings page.
 * @param settings Reference to the global settings struct.
 * @param throttle Channel 1 value (0-255).
 * @param pitch Channel 2 value (0-255).
 * @param roll Channel 3 value (0-255).
 * @param yaw Channel 4 value (0-255).
 * @param aux1 Channel 5 value.
 * @param aux2 Channel 6 value.
 * @param aux3 Channel 7 state.
 * @param aux4 Channel 8 state.
 * @param voltage Converted battery voltage.
 * @param timerSelection Selected timer duration (minutes).
 * @param timerIsArmed Is the timer ready to start?
 * @param timerIsRunning Is the countdown active?
 * @param timerValue Remaining milliseconds on the timer.
 * @param isTimeEditMode Is the user currently changing the timer duration?
 * @param invertMenuIndex Cursor position for the Inversion page.
 */
void drawCurrentPage(
    DisplayState currentPage,
    int trimsMenuIndex,
    int settingsMenuIndex,
    int featuresMenuIndex,
    const RadioSettings& settings,
    uint16_t throttle, uint16_t pitch, uint16_t roll, uint16_t yaw,
    byte aux1, byte aux2, bool aux3, bool aux4,
    float voltage,
    int timerSelection,
    bool timerIsArmed,
    bool timerIsRunning,
    long timerValue,
    bool isTimeEditMode,
    int invertMenuIndex,
    int drMenuIndex,
    int advChannelSelectIndex,
    int advConfigMenuIndex,
    int currentEditingChannel,
    bool isAdvEditMode,
    int expoMenuIndex,
    bool isExpoEditMode 

);

#endif // DISPLAY_MANAGER_H