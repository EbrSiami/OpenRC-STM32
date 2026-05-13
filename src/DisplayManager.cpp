/**
 * @file DisplayManager.cpp
 * @author Ebrahim Siami
 * @brief OLED UI & Graphics Engine
 * @version 4.0.1
 * @date 2026-04-23
 * 
 * Manages all graphical user interface (GUI) elements including:
 * - Boot splash screen animation
 * - Menu navigation and page rendering
 * - Real-time telemetry display (Battery, Timer, Channels)
 * - Trim visualizers
 * - Advanced Settings Menus (Inversion, Modes)
 * - and much more that im lazy to write here for now
 */

#include "DisplayManager.h"
#include <Wire.h>
#include "Buzzer.h"
#include "Radio.h"

// =============================================================================
// --- Graphics Assets ---
// =============================================================================

// Bitmap Data: MIG-21 Fighter Jet (64x32 pixels)
const unsigned char epd_bitmap_mig_21 [] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x80, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x80, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x07, 0xfe, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xfe, 0x00, 0x0e, 0x00,
    0x00, 0x00, 0x00, 0x3f, 0xff, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0xfc, 0x00,
    0x00, 0x00, 0x03, 0xff, 0xff, 0x83, 0xf8, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x00,
    0x00, 0xfd, 0xfb, 0xff, 0xff, 0xff, 0xf0, 0x00, 0x00, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x00,
    0x00, 0x00, 0x03, 0xff, 0xff, 0x87, 0xf8, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x01, 0xfc, 0x00,
    0x00, 0x00, 0x00, 0x3f, 0xff, 0x00, 0x7c, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xff, 0x00, 0x1e, 0x00,
    0x00, 0x00, 0x00, 0x0f, 0xff, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// =============================================================================
// --- very strange externs! ---
// =============================================================================
extern bool simulatorMode;
extern bool isDREditMode;
extern uint8_t calibStep;

// =============================================================================
// --- Initialization & Helper Functions ---
// =============================================================================

void setupDisplay() {
    Wire.begin();
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        for(;;); // Halt execution if OLED fails
    }

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(3);

    const char* introText = "EBR.co";
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(introText, 0, 0, &x1, &y1, &w, &h);

    // Center text
    int textX = (SCREEN_WIDTH - w) / 2;
    int textY = (SCREEN_HEIGHT - h) / 2;

    display.setCursor(textX, textY);
    display.println(introText);

    // Draw frame around text
    int padding = 4;
    display.drawRoundRect(textX - padding, textY - padding, w + (2 * padding), h + (2 * padding), 4, SSD1306_WHITE);

    display.display();
    delay(2000);
}

void showSavingFeedback() {
    display.clearDisplay();
    display.setTextSize(2);

    const char* text = "Saving...";
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

    display.setCursor((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2);
    display.print(text);
    display.display();

    playBeepEvent(EVT_CONFIRM);
    delay(300); // Allow time for EEPROM write cycle
}

void drawBar(const char* label, int x, int y, uint16_t value, uint16_t maxValue) {
    display.setCursor(x, y);
    display.print(label);
    
    int barWidth = 80;
    // i hate this map function
    // int filled = map(value, 0, maxValue, 0, barWidth);
    // int filled = map((long)value, 0L, (long)maxValue, 0L, (long)barWidth);
    int filled = (int)((uint32_t)value * barWidth / maxValue);  // hope this one works 
    
    display.drawRect(x + 30, y, barWidth, 8, SSD1306_WHITE);
    display.fillRect(x + 30, y, filled, 8, SSD1306_WHITE);
}

/**
 * @brief Displays the boot animation with a moving jet and progress bar.
 */
void showSplashScreen(const char* productName, const unsigned long durationMs) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    
    const char* smallLogoText = "EBR.co";
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(smallLogoText, 0, 0, &x1, &y1, &w, &h);

    const int FINAL_PLANE_X = (SCREEN_WIDTH - 64) / 2;
    const int FINAL_PLANE_Y = (SCREEN_HEIGHT / 2) - 18;
    int loadingBarY = SCREEN_HEIGHT - 10;
    int loadingBarWidth = SCREEN_WIDTH - 20;
    int loadingBarX = 10;

    unsigned long startTime = millis();
    unsigned long animationDuration = durationMs / 2;
    if (animationDuration == 0) animationDuration = 1;

    // Phase 1: Animation - Plane flies in
    for (unsigned long elapsed = 0; elapsed <= animationDuration; elapsed = millis() - startTime) {
        display.clearDisplay();
        
        // Draw Header
        display.setTextSize(1);
        display.setCursor((SCREEN_WIDTH - w) / 2, 2);
        display.println(smallLogoText);

        // Draw Plane (Moving)
        int currentPlaneX = map(elapsed, 0, animationDuration, SCREEN_WIDTH, FINAL_PLANE_X);
        display.drawBitmap(currentPlaneX, FINAL_PLANE_Y, epd_bitmap_mig_21, 64, 32, SSD1306_WHITE);

        // Draw Progress Bar
        int filledWidth = map(elapsed, 0, durationMs, 0, loadingBarWidth);
        display.drawRect(loadingBarX, loadingBarY, loadingBarWidth, 8, SSD1306_WHITE);
        display.fillRect(loadingBarX, loadingBarY, filledWidth, 8, SSD1306_WHITE);
        
        display.display();
    }

    // Phase 2: Static Hold - Wait for remaining duration
    unsigned long currentMillis = millis();
    while (currentMillis < startTime + durationMs) {
        display.clearDisplay();
        
        display.setTextSize(1);
        display.setCursor((SCREEN_WIDTH - w) / 2, 2);
        display.println(smallLogoText);
        
        display.drawBitmap(FINAL_PLANE_X, FINAL_PLANE_Y, epd_bitmap_mig_21, 64, 32, SSD1306_WHITE);
        
        unsigned long elapsed = currentMillis - startTime;
        int filledWidth = map(elapsed, 0, durationMs, 0, loadingBarWidth);
        display.drawRect(loadingBarX, loadingBarY, loadingBarWidth, 8, SSD1306_WHITE);
        display.fillRect(loadingBarX, loadingBarY, filledWidth, 8, SSD1306_WHITE);
        
        display.display();
        currentMillis = millis();
    }
    display.clearDisplay();
}

// ==========================================
// -- Helper: Draw Trim Indicator --
// ==========================================
static void drawTrimIndicator(int x, int y, int trimValue) {
    const int CENTER = 2048;
    const int TOLERANCE = 50;  // Tolerance for "centered"
    
    if (abs(trimValue - CENTER) <= TOLERANCE) {
        // Trim is centered - draw filled circle
        display.fillCircle(x, y, 2, SSD1306_WHITE);
    } else if (trimValue > CENTER) {
        // Trim is above center - draw up arrow
        display.drawPixel(x, y - 2, SSD1306_WHITE);     // top point
        display.drawPixel(x - 1, y - 1, SSD1306_WHITE); // left
        display.drawPixel(x + 1, y - 1, SSD1306_WHITE); // right
        display.drawPixel(x, y, SSD1306_WHITE);         // middle
        display.drawPixel(x, y + 1, SSD1306_WHITE);     // tail
    } else {
        // Trim is below center - draw down arrow
        display.drawPixel(x, y + 2, SSD1306_WHITE);     // bottom point
        display.drawPixel(x - 1, y + 1, SSD1306_WHITE); // left
        display.drawPixel(x + 1, y + 1, SSD1306_WHITE); // right
        display.drawPixel(x, y, SSD1306_WHITE);         // middle
        display.drawPixel(x, y - 1, SSD1306_WHITE);     // tail
    }
}

// =============================================================================
// --- Main Rendering Engine ---
// =============================================================================

static void drawNavFooter(int leftIndex, int rightIndex, int currentIndex) {
    int y = 54;

    // Left button
    if (leftIndex >= 0) {
        if (currentIndex == leftIndex) {
            display.fillRect(0, y, 20, 8, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
        } else {
            display.setTextColor(SSD1306_WHITE);
        }
        display.setCursor(2, y);
        display.print("<<");
    }

    // Right button
    if (rightIndex >= 0) {
        if (currentIndex == rightIndex) {
            display.fillRect(SCREEN_WIDTH - 20, y, 20, 8, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
        } else {
            display.setTextColor(SSD1306_WHITE);
        }
        display.setCursor(SCREEN_WIDTH - 15, y);
        display.print(">>");
    }

    display.setTextColor(SSD1306_WHITE); // reset
}

void drawCurrentPage(
    DisplayState currentPage,
    int trimsMenuIndex,
    int settingsMenuIndex,
    int featuresMenuIndex,
    const RadioSettings& settings,
    uint16_t throttle, uint16_t pitch, uint16_t roll, uint16_t yaw,
    byte aux1, byte aux2, bool aux3, bool aux4,
    float voltage,
    int timerSelection, bool timerIsArmed, bool timerIsRunning, long timerValue, bool isTimeEditMode,
    int invertMenuIndex, int drMenuIndex, int advChannelSelectIndex, int advConfigMenuIndex, 
    int currentEditingChannel, bool isAdvEditMode, int expoMenuIndex, bool isExpoEditMode
) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.invertDisplay(settings.lightModeEnabled);

    String pageDisplayName = "";
    int16_t boundX, boundY;
    uint16_t boundW, boundH;
    int navOptionsY = SCREEN_HEIGHT - 9;

    switch (currentPage) {

        // ---------------------------------------------------------------------
        // --- PAGE: SYSTEM DASHBOARD (Main3) ---
        // ---------------------------------------------------------------------
        case PAGE_MAIN3: {
            pageDisplayName = "System";

            // ==========================================
            // -- Y-Coordinates --
            // ==========================================
            int topY = 2;        // battery & TX status
            int row2Y = 22;      // timer and D/R
            int row3Y = 38;      // trim status & mix mode

            // ==========================================
            // -- Battery Logic --
            // ==========================================
            const float BATT_MAX_VOLTAGE = 8.4;
            const float BATT_MIN_VOLTAGE = 7.2;
            long level = constrain(map(voltage * 100, (long)(BATT_MIN_VOLTAGE * 100), (long)(BATT_MAX_VOLTAGE * 100), 0, 100), 0, 100);
            
            int battX = 5, battY = topY, battWidth = 28, battHeight = 12;
            display.drawRect(battX, battY, battWidth, battHeight, SSD1306_WHITE);
            display.fillRect(battX + battWidth, battY + 3, 3, battHeight - 6, SSD1306_WHITE);
            
            int fillWidth = map(level, 0, 100, 0, battWidth - 2);
            if (fillWidth > 0) display.fillRect(battX + 1, battY + 1, fillWidth, battHeight - 2, SSD1306_WHITE);
            
            display.setTextSize(1);
            display.setCursor(battX + battWidth + 8, battY + 2);
            display.print(voltage, 2);
            display.print("V");

            // ==========================================
            // -- Radio Status Indicator --
            // ==========================================
            display.setCursor(80, topY + 2);
            if (getRadioStatus()) {
                display.print("TX:OK");
            } else {
                // Blink the error so the user notices!
                if (millis() % 1000 < 500) {
                    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
                    display.print("!TX ERR!");
                    display.setTextColor(SSD1306_WHITE);
                } else {
                    display.print("       ");
                }
            }

            // ==========================================
            // -- Timer Logic & Text Formatting --
            // ==========================================
            char timeText[10];

            // 1- if the user is in timer edit mode
            if (isTimeEditMode) {
                if (timerSelection == -1) {
                    sprintf(timeText, "--:--");
                } else {
                    sprintf(timeText, "%02d:00", timerSelection); 
                }
            } 
            // 2- if user is not in edit mode and timer is not armed
            else if (timerSelection == -1) {
                sprintf(timeText, "--:--");
            } 
            // 3- timer is activated or counting or waiting 
            else {
                if (timerIsArmed || timerIsRunning) {
                    long remaining = timerValue;
                    if (remaining >= 0) {
                        unsigned long minutes = (unsigned long)remaining / 60000;
                        unsigned long seconds = ((unsigned long)remaining / 1000) % 60;
                        sprintf(timeText, "%02lu:%02lu", minutes, seconds);
                    } else {
                        // Count up if timer expired
                        unsigned long passed = -remaining;
                        unsigned long minutes = passed / 60000;
                        unsigned long seconds = (passed / 1000) % 60;
                        sprintf(timeText, "+%02lu:%02lu", minutes, seconds); 
                    }
                } else {
                    sprintf(timeText, "%02d:00", timerSelection);
                }
            }

            // ==========================================
            // -- Timer Display (Left Side) --
            // ==========================================
            display.setCursor(5, row2Y);
            
            // Highlight ONLY the Timer text (Index 2)
            if (settingsMenuIndex == 2) {
                display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
            } else {
                display.setTextColor(SSD1306_WHITE);
            }

            display.print("TIMER:");

            // Blink effect when editing
            bool shouldShowTime = true;
            if (settingsMenuIndex == 2 && isTimeEditMode && (millis() % 1000 < 500)) {
                shouldShowTime = false;
            }

            if (shouldShowTime) {
                display.print(timeText);
            } else {
                display.print("     "); // Print blank spaces so the highlight box doesn't vanish!
            }
            display.setTextColor(SSD1306_WHITE); // Reset color

            // ==========================================
            // -- D/R Toggle Display (Right Side) --
            // ==========================================
            display.setCursor(85, row2Y); 
            
            // Highlight if selected (Index 3)
            if (settingsMenuIndex == 3) {
                display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); 
            } else {
                display.setTextColor(SSD1306_WHITE);
            }

            // Print ON or OFF
            if (settings.dualRateEnabled) {
                display.print("D/R:ON "); // Extra space for clearing pixels
            } else {
                display.print("D/R:OFF");
            }
            display.setTextColor(SSD1306_WHITE); // Reset text color

            // ==========================================
            // -- Trim Status & Mix Mode Indicator --
            // ==========================================
            display.setCursor(15, row3Y);
            display.setTextColor(SSD1306_WHITE);
            display.print("T:");
            
            // Draw trim indicators (T1, T2, T3)
            drawTrimIndicator(35, row3Y + 3, settings.trim1);
            drawTrimIndicator(45, row3Y + 3, settings.trim2);
            drawTrimIndicator(55, row3Y + 3, settings.trim3);
            
            // Draw mix mode on the right side
            display.setCursor(75, row3Y);
            display.print("MIX:");
            const char* mixNames[] = {"NRM", "VT A", "VT B", "DL A", "DL B"};
            if (settings.mixMode >= 0 && settings.mixMode <= 4) {
                display.print(mixNames[settings.mixMode]);
            }

            // ==========================================
            // -- Navigation Footer --
            // ==========================================
            drawNavFooter(-1, 0, settingsMenuIndex);
            break;
        }

        // ---------------------------------------------------------------------
        // --- PAGE: CHANNELS 1-4 (Main1) ---
        // ---------------------------------------------------------------------
        case PAGE_MAIN1: {
            drawBar("THT:", 0, 0, throttle, 2047);
            drawBar("PIT:", 0, 12, pitch, 2047);
            drawBar("ROL:", 0, 24, roll, 2047);
            drawBar("YAW:", 0, 36, yaw, 2047);

            drawNavFooter(1, 0, settingsMenuIndex);
            
            pageDisplayName = "Channels 1-4";
            break;
        }

        // ---------------------------------------------------------------------
        // --- PAGE: CHANNELS 5-8 (Main2) ---
        // ---------------------------------------------------------------------
        case PAGE_MAIN2: {
            drawBar("AUX1:", 0, 0, aux1, 255);
            drawBar("AUX2:", 0, 12, aux2, 255);
            drawBar("AUX3:", 0, 24, aux3 ? 1 : 0, 1);
            drawBar("AUX4:", 0, 36, aux4 ? 1 : 0, 1);

            drawNavFooter(1, 0, settingsMenuIndex);
            
            pageDisplayName = "Channels 5-8";
            break;
        }

        // ---------------------------------------------------------------------
        // --- PAGE: TRIM ADJUSTMENT ---
        // ---------------------------------------------------------------------
        case PAGE_TRIMS: {
            // -- Trim 1 Visualization --
            int trimY1 = 3;
            display.setCursor(0, trimY1); display.print("T1:");
            int lineX1 = 25; int lineY1 = trimY1 + 4; int lineWidth = 70;
            
            display.drawLine(lineX1, lineY1, lineX1 + lineWidth, lineY1, SSD1306_WHITE); // Axis line
            display.drawFastVLine(map(2048, 0, 4095, lineX1, lineX1 + lineWidth), lineY1 - 2, 5, SSD1306_WHITE); // Center tick
            display.fillRect(map(settings.trim1, 0, 4095, lineX1, lineX1 + lineWidth) - 1, lineY1 - 3, 3, 7, SSD1306_WHITE); // Cursor
            
            display.setCursor(lineX1 + lineWidth + 5, trimY1);
            display.print(map(settings.trim1, 0, 4095, 0, 100)); display.print("%");

            // -- Trim 2 Visualization --
            int trimY2 = 16;
            display.setCursor(0, trimY2); display.print("T2:");
            int lineX2 = 25; int lineY2 = trimY2 + 4;
            
            display.drawLine(lineX2, lineY2, lineX2 + lineWidth, lineY2, SSD1306_WHITE);
            display.drawFastVLine(map(2048, 0, 4095, lineX2, lineX2 + lineWidth), lineY2 - 2, 5, SSD1306_WHITE);
            display.fillRect(map(settings.trim2, 0, 4095, lineX2, lineX2 + lineWidth) - 1, lineY2 - 3, 3, 7, SSD1306_WHITE);
            
            display.setCursor(lineX2 + lineWidth + 5, trimY2);
            display.print(map(settings.trim2, 0, 4095, 0, 100)); display.print("%");

            // -- Trim 3 Visualization --
            int trimY3 = 29;
            display.setCursor(0, trimY3); display.print("T3:");
            int lineX3 = 25; int lineY3 = trimY3 + 4;
            
            display.drawLine(lineX3, lineY3, lineX3 + lineWidth, lineY3, SSD1306_WHITE);
            display.drawFastVLine(map(2048, 0, 4095, lineX3, lineX3 + lineWidth), lineY3 - 2, 5, SSD1306_WHITE);
            display.fillRect(map(settings.trim3, 0, 4095, lineX3, lineX3 + lineWidth) - 1, lineY3 - 3, 3, 7, SSD1306_WHITE);
            
            display.setCursor(lineX3 + lineWidth + 5, trimY3);
            display.print(map(settings.trim3, 0, 4095, 0, 100)); display.print("%");

            // -- "Save Trims" Button --
            int resetTrimsY = 42;
            const char* resetText = "Save Trims";
            int16_t rX, rY; uint16_t rW, rH;
            display.getTextBounds(resetText, 0, 0, &rX, &rY, &rW, &rH);
            int resetTrimsX = (SCREEN_WIDTH - rW) / 2;
            display.setCursor(resetTrimsX, resetTrimsY);

            if (trimsMenuIndex == 0) {
                display.fillRect(0, resetTrimsY, SCREEN_WIDTH, 8, SSD1306_WHITE);
                display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
            } else {
                display.setTextColor(SSD1306_WHITE);
            }
            display.print(resetText);

            // Footer Navigation
            drawNavFooter(2, 1, trimsMenuIndex);
            
            pageDisplayName = "Trim Adjust";
            break;
        }

        // ---------------------------------------------------------------------
        // --- PAGE: SETTINGS MENU ---
        // ---------------------------------------------------------------------
        case MENU: {
            pageDisplayName = "Settings";
            display.setTextSize(1);

            for (int i = 0; i < SETTING_TOTAL - 2; i++) {
                int y = 8 * i + 4;
                if (i == settingsMenuIndex) {
                    display.fillRect(0, y - 1, SCREEN_WIDTH, 9, SSD1306_WHITE);
                    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
                } else {
                    display.setTextColor(SSD1306_WHITE);
                }

                display.setCursor(5, y);
                switch(i) {
                    case SETTING_LIGHT_MODE:
                        display.print("Light Mode: "); display.print(settings.lightModeEnabled ? "On" : "Off"); break;
                    case SETTING_BUZZER:
                        display.print("Buzzer: "); display.print(settings.buzzerEnabled ? "On" : "Off"); break;
                    case SETTING_CH_INVERT:
                        display.print("Channel Invert >"); break;
                    case SETTING_RESET_TRIMS:
                        display.print("Reset Trims"); break;
                    case SETTING_THROTTLE_MODE:
                        display.print("Thr Mode: ");
                        display.print(settings.airplaneMode ? "AIR" : "NRM");
                        break;
                    case SETTING_INFO:
                        display.print("About / Info >"); break;
                }
            }

            // Footer Navigation
            drawNavFooter(SETTING_BACK, SETTING_NEXT, settingsMenuIndex);

            break;
        }

        // ---------------------------------------------------------------------
        // --- PAGE: FEATURES MENU ---
        // ---------------------------------------------------------------------
        case PAGE_FEATURES: {
            pageDisplayName = "Features";
            display.setTextSize(1);

            for (int i = 0; i < FEATURE_BACK; i++) {
                int y = 8 * i + 4;
                if (i == featuresMenuIndex) {
                    display.fillRect(0, y - 1, SCREEN_WIDTH, 9, SSD1306_WHITE);
                    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
                } else {
                    display.setTextColor(SSD1306_WHITE);
                }

                display.setCursor(5, y);
                switch(i) {
                    case FEATURE_EXPO:              display.print("Expo >"); break;
                    case FEATURE_DUAL_RATE:         display.print("Dual Rate >"); break;
                    case FEATURE_CHANNEL_ADVANCED:  display.print("Channel Advanced >"); break;
                    case FEATURE_CALIBRATION:       display.print("Calibration >"); break;
                    case FEATURE_CHANNELS_MIX: {
                        display.print("Channels Mix: ");
                        const char* mixNames[] = {"Normal", "VTL A", "VTL B", "DLT A", "DLT B"};
                        if (settings.mixMode >= 0 && settings.mixMode <= 4) {
                            display.print(mixNames[settings.mixMode]);
                        } else {     
                            display.print("Unknown");
                        }
                        break; // damnit i forgot to add a {} here haha, jews fault!
                }
                    case FEATURE_SIMULATOR:
                        display.print("Simulator Mode: ");
                        display.print(simulatorMode ? "On" : "Off");
                }
            }

            // Back Button
            drawNavFooter(FEATURE_BACK, -1, featuresMenuIndex);
            break;
        }

        // ---------------------------------------------------------------------
        // --- PAGE: DUAL RATE ---
        // ---------------------------------------------------------------------
        case PAGE_DUAL_RATE: {
            display.setTextSize(1);

            auto drawDRBar = [](int y, int percent) {
                int barWidth = map(constrain(percent, 0, 100), 0, 100, 0, 40);
                display.drawRect(80, y, 42, 7, SSD1306_WHITE);
                display.fillRect(81, y + 1, barWidth, 5, SSD1306_WHITE);
            };

            // --- ROLL (Index 2) ---
            if (drMenuIndex == 2) {
                display.fillRoundRect(2, 4, 70, 11, 2, SSD1306_WHITE);
                display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
            } else { display.setTextColor(SSD1306_WHITE); }
            
            display.setCursor(6, 6);
            display.print("Roll: ");
            if (!(drMenuIndex == 2 && isDREditMode && (millis() % 1000 < 500))) {
                display.setCursor(45, 6);
                if(settings.dualRateRoll < 100) display.print(" ");
                display.print(settings.dualRateRoll); display.print("%");
            }
            drawDRBar(6, settings.dualRateRoll);

            // --- PITCH (Index 3) ---
            if (drMenuIndex == 3) {
                display.fillRoundRect(2, 16, 70, 11, 2, SSD1306_WHITE);
                display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
            } else { display.setTextColor(SSD1306_WHITE); }
            
            display.setCursor(6, 18);
            display.print("Pitch:");
            if (!(drMenuIndex == 3 && isDREditMode && (millis() % 1000 < 500))) {
                display.setCursor(45, 18);
                if(settings.dualRatePitch < 100) display.print(" ");
                display.print(settings.dualRatePitch); display.print("%");
            }
            drawDRBar(18, settings.dualRatePitch);

            // --- YAW (Index 4) ---
            if (drMenuIndex == 4) {
                display.fillRoundRect(2, 28, 70, 11, 2, SSD1306_WHITE);
                display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
            } else { display.setTextColor(SSD1306_WHITE); }
            
            display.setCursor(6, 30);
            display.print("Yaw:  ");
            if (!(drMenuIndex == 4 && isDREditMode && (millis() % 1000 < 500))) {
                display.setCursor(45, 30);
                if(settings.dualRateYaw < 100) display.print(" ");
                display.print(settings.dualRateYaw); display.print("%");
            }
            drawDRBar(30, settings.dualRateYaw);

            display.drawFastHLine(0, 43, SCREEN_WIDTH, SSD1306_WHITE);

            // --- SAVE & BACK BUTTONS ---
            display.setTextColor(SSD1306_WHITE);
            
            if (drMenuIndex == 0) {
                display.fillRoundRect(4, 48, 50, 13, 3, SSD1306_WHITE);
                display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
            } else { display.drawRoundRect(4, 48, 50, 13, 3, SSD1306_WHITE); }
            display.setCursor(12, 51); display.print("BACK");
            
            display.setTextColor(SSD1306_WHITE);
            if (drMenuIndex == 5) {
                display.fillRoundRect(64, 48, 60, 13, 3, SSD1306_WHITE);
                display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
            } else { display.drawRoundRect(64, 48, 60, 13, 3, SSD1306_WHITE); }
            display.setCursor(82, 51); display.print("SAVE");

            break;
        }

        // ---------------------------------------------------------------------
        // --- PAGE: INFO / CREDITS ---
        // ---------------------------------------------------------------------
        case PAGE_INFO: {
            display.setTextSize(1);

            display.drawRoundRect(0, 0, 128, 64, 5, SSD1306_WHITE);
            
            display.fillRoundRect(10, 4, 108, 13, 2, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
            display.setCursor(20, 7);
            display.println("EBR.co E_I8L RC");
            
            display.setTextColor(SSD1306_WHITE);
            
            display.setCursor(17, 22);
            display.println("Firmware: V2.6.1");
            
            display.setCursor(11, 34);
            display.println("Tel: +3--141592653");
            
            display.drawLine(10, 46, 118, 46, SSD1306_WHITE);
            
            display.setCursor(11, 51);
            display.println("[ENTER] to go back");
            break;
        }

        // ---------------------------------------------------------------------
        // --- PAGE: CHANNEL INVERSION ---
        // ---------------------------------------------------------------------
        case PAGE_CH_INVERT: {
            display.setTextSize(1);

            // Two-column layout: Left (CH1-4), Right (CH5-8)
            for (int i = 0; i < 8; i++) {
                int x = (i < 4) ? 2 : 66;
                int y = (i % 4) * 11 + 2;

                if (i == invertMenuIndex) {
                    display.fillRoundRect(x, y, 60, 10, 2, SSD1306_WHITE);
                    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
                } else {
                    display.drawRoundRect(x, y, 60, 10, 2, SSD1306_WHITE);
                    display.setTextColor(SSD1306_WHITE);
                }

                display.setCursor(x + 4, y + 1);
                display.print("CH");
                display.print(i + 1);
                display.print(":");
                
                display.setCursor(x + 36, y + 1);
                display.print(settings.channelInverted[i] ? "INV" : "NRM");
            }

            // Back Button at the bottom
            int backY = 48;
            if (invertMenuIndex == 8) {
                display.fillRoundRect(34, backY, 60, 11, 3, SSD1306_WHITE);
                display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
            } else {
                display.drawRoundRect(34, backY, 60, 11, 3, SSD1306_WHITE);
                display.setTextColor(SSD1306_WHITE);
            }
            display.setCursor(44, backY + 2);
            display.print("<< BACK");

            break;
        }

        // ---------------------------------------------------------------------
        // --- PAGE: CALIBRATION ---
        // ---------------------------------------------------------------------
        case PAGE_CALIBRATION: {
            display.setTextSize(1);

            display.drawRoundRect(0, 0, 128, 13, 2, SSD1306_WHITE);
            display.setCursor(4, 3);
            display.print("Status: ");
            if (calibStep == 0) display.print("READY");
            else if (calibStep == 3) display.print("FINISHED");
            else { display.print("STEP "); display.print(calibStep); display.print("/2"); }

            display.drawRoundRect(0, 16, 128, 48, 3, SSD1306_WHITE);

            if (calibStep == 0) {
                display.setCursor(25, 24);
                display.print("PRESS [ENTER]");
                display.setCursor(13, 34);
                display.print("Begin Calibration");
                
                display.fillRoundRect(4, 49, 120, 12, 2, SSD1306_WHITE);
                display.setTextColor(SSD1306_BLACK);
                display.setCursor(10, 51);
                display.print("UP/DOWN: Exit Menu");
                display.setTextColor(SSD1306_WHITE);
            }
            else if (calibStep == 1) {
                display.setCursor(5, 25);
                display.print("1.Center all sticks");
                display.setCursor(5, 38);
                display.print("2.Press [OK]");
                display.setCursor(32, 52);
                display.print("Calibration");
            } 
            else if (calibStep == 2) {
                display.setCursor(10, 20);
                display.print("Move all sticks to");
                display.setCursor(10, 30);
                display.print("MAX/MIN extremes");
                display.setCursor(20, 40);
                display.print("Then press [OK]");
                display.setCursor(32, 52);
                display.print("Calibration");
            } 
            else if (calibStep == 3) {
                display.setCursor(30, 25);
                display.print("CALIB SAVED!");
                display.fillRoundRect(15, 38, 98, 12, 2, SSD1306_WHITE);
                display.setTextColor(SSD1306_BLACK);
                display.setCursor(17, 40);
                display.print("Press OK to Exit");
                display.setTextColor(SSD1306_WHITE);
                display.setCursor(32, 52);
                display.print("Calibration");
            }
            break;
        }

        // ---------------------------------------------------------------------
        // --- PAGE: ADVANCED CHANNELS SELECT ---
        // ---------------------------------------------------------------------
        case PAGE_CHANNELS_ADVANCED: {
            pageDisplayName = "Adv. Channels";
            display.setTextSize(1);

            const char* channelNames[] = {"1. Roll", "2. Pitch", "3. Throttle", "4. Yaw"};

            for (int i = 0; i < 4; i++) {
                int y = 8 * i + 4;
                if (i == advChannelSelectIndex) {
                    display.fillRoundRect(0, y - 1, SCREEN_WIDTH, 9, 2, SSD1306_WHITE);
                    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
                } else {
                    display.setTextColor(SSD1306_WHITE);
                }
                display.setCursor(5, y);
                display.print(channelNames[i]);
            }

            display.drawFastHLine(0, 40, SCREEN_WIDTH, SSD1306_WHITE);

            int backY = 44;
            if (advChannelSelectIndex == 4) {
                display.fillRoundRect(0, backY, SCREEN_WIDTH, 10, 2, SSD1306_WHITE);
                display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
            } else {
                display.setTextColor(SSD1306_WHITE);
                display.drawRoundRect(0, backY, SCREEN_WIDTH, 10, 2, SSD1306_WHITE);
            }
            display.setCursor(SCREEN_WIDTH/2 - 15, backY + 1);
            display.print("BACK");
            display.setTextColor(SSD1306_WHITE); // ah i missed this thing here.
            // fuck zionists and jews forever
            break;
        }

        // ---------------------------------------------------------------------
        // --- PAGE: CHANNEL CONFIG (EPA & SUBTRIM) ---
        // ---------------------------------------------------------------------
        case PAGE_CHANNEL_CONFIG: {
            const char* chNames[] = {"Roll", "Pitch", "Throttle", "Yaw"};
            pageDisplayName = chNames[currentEditingChannel];
            display.setTextSize(1);

            auto drawBar = [&](int y, int value) {
                int barWidth = 40;
                int barX = 85;
                display.drawRect(barX, y, barWidth, 7, SSD1306_WHITE);
                int fill = map(value, 0, 4095, 0, barWidth - 2);
                fill = constrain(fill, 0, barWidth - 2);
                display.fillRect(barX + 1, y + 1, fill, 5, SSD1306_WHITE);
            };

            // --- MIN (EPA) - Index 1 ---
            display.setCursor(2, 6);
            if (advConfigMenuIndex == 1) display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
            else display.setTextColor(SSD1306_WHITE);
            display.print(" MIN: ");
            display.setTextColor(SSD1306_WHITE);
            
            if (!(advConfigMenuIndex == 1 && isAdvEditMode && (millis() % 1000 < 500))) {
                display.setCursor(40, 6);
                float minPct = (settings.epaMin[currentEditingChannel] / 4095.0) * 100.0;
                display.print(minPct, 1);
                display.print("%");
            }
            drawBar(5, settings.epaMin[currentEditingChannel]);

            // --- CENTER (SUBTRIM) - Index 2 ---
            display.setCursor(2, 18);
            if (advConfigMenuIndex == 2) display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
            else display.setTextColor(SSD1306_WHITE);
            display.print(" MID: ");
            display.setTextColor(SSD1306_WHITE);
            
            if (!(advConfigMenuIndex == 2 && isAdvEditMode && (millis() % 1000 < 500))) {
                display.setCursor(40, 18);
                float midPct = (settings.subTrim[currentEditingChannel] / 4095.0) * 100.0;
                display.print(midPct, 1);
                display.print("%");
            }
            drawBar(17, settings.subTrim[currentEditingChannel]);

            // --- MAX (EPA) - Index 3 ---
            display.setCursor(2, 30);
            if (advConfigMenuIndex == 3) display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
            else display.setTextColor(SSD1306_WHITE);
            display.print(" MAX: ");
            display.setTextColor(SSD1306_WHITE);
            
            if (!(advConfigMenuIndex == 3 && isAdvEditMode && (millis() % 1000 < 500))) {
                display.setCursor(40, 30);
                float maxPct = (settings.epaMax[currentEditingChannel] / 4095.0) * 100.0;
                display.print(maxPct, 1);
                display.print("%");
            }
            drawBar(29, settings.epaMax[currentEditingChannel]);

            // --- SEPARATE SAVE & BACK BUTTONS ---
            int btnY = 41;
            
            if (advConfigMenuIndex == 4) {
                display.fillRoundRect(5, btnY, 55, 11, 2, SSD1306_WHITE);
                display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
            } else {
                display.drawRoundRect(5, btnY, 55, 11, 2, SSD1306_WHITE);
                display.setTextColor(SSD1306_WHITE);
            }
            display.setCursor(20, btnY + 2);
            display.print("SAVE");

            if (advConfigMenuIndex == 0) {
                display.fillRoundRect(68, btnY, 55, 11, 2, SSD1306_WHITE);
                display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
            } else {
                display.drawRoundRect(68, btnY, 55, 11, 2, SSD1306_WHITE);
                display.setTextColor(SSD1306_WHITE);
            }
            display.setCursor(83, btnY + 2);
            display.print("BACK");

            break;
        }

        // ---------------------------------------------------------------------
        // --- PAGE: EXPO ---
        // ---------------------------------------------------------------------
        case PAGE_EXPO: {
            display.setTextSize(1);
            int xPos;

            auto drawExpoBar = [](int y, uint16_t channelValue) {
                int barWidth = map(channelValue, 0, 2047, 0, 40);
                display.drawRect(80, y, 42, 7, SSD1306_WHITE);
                display.fillRect(81, y + 1, barWidth, 5, SSD1306_WHITE);
            };

            // --- ROLL (Index 1) ---
            if (expoMenuIndex == 1) {
                display.fillRoundRect(2, 4, 70, 11, 2, SSD1306_WHITE);
                display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
            } else { display.setTextColor(SSD1306_WHITE); }

            display.setCursor(6, 6);
            display.print("Roll:");

            if (!(expoMenuIndex == 1 && isExpoEditMode && (millis() % 1000 < 500))) {
                String valStr = String(settings.expoRoll) + "%";
                int strWidth = valStr.length() * 6;
                display.setCursor(68 - strWidth, 6); 
                display.print(valStr);
            }
            drawExpoBar(6, roll);

            // --- PITCH (Index 2) ---
            if (expoMenuIndex == 2) {
                display.fillRoundRect(2, 16, 70, 11, 2, SSD1306_WHITE);
                display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
            } else { display.setTextColor(SSD1306_WHITE); }

            display.setCursor(6, 18);
            display.print("Pitch:");

            if (!(expoMenuIndex == 2 && isExpoEditMode && (millis() % 1000 < 500))) {
                String valStr = String(settings.expoPitch) + "%";
                int strWidth = valStr.length() * 6;
                display.setCursor(68 - strWidth, 18); 
                display.print(valStr);
            }
            drawExpoBar(18, pitch);

            // --- YAW (Index 3) ---
            if (expoMenuIndex == 3) {
                display.fillRoundRect(2, 28, 70, 11, 2, SSD1306_WHITE);
                display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
            } else { display.setTextColor(SSD1306_WHITE); }

            display.setCursor(6, 30);
            display.print("Yaw:");

            if (!(expoMenuIndex == 3 && isExpoEditMode && (millis() % 1000 < 500))) {
                String valStr = String(settings.expoYaw) + "%";
                int strWidth = valStr.length() * 6;
                display.setCursor(68 - strWidth, 30); 
                display.print(valStr);
            }
            drawExpoBar(30, yaw);

            display.drawFastHLine(0, 43, SCREEN_WIDTH, SSD1306_WHITE);

            // --- SAVE & BACK BUTTONS ---
            display.setTextColor(SSD1306_WHITE);
            
            // BACK botton (Index 0)
            if (expoMenuIndex == 0) {
                display.fillRoundRect(4, 48, 50, 13, 3, SSD1306_WHITE);
                display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
            } else { display.drawRoundRect(4, 48, 50, 13, 3, SSD1306_WHITE); }
            display.setCursor(12, 51); display.print("BACK");
            
            display.setTextColor(SSD1306_WHITE);

            // SAVE botton (Index 4)
            if (expoMenuIndex == 4) {
                display.fillRoundRect(64, 48, 60, 13, 3, SSD1306_WHITE);
                display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
            } else { display.drawRoundRect(64, 48, 60, 13, 3, SSD1306_WHITE); }
            display.setCursor(82, 51); display.print("SAVE");

            break;
        }
    }

    // --- Footer: Draw Page Name Centered ---
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.getTextBounds(pageDisplayName, 0, 0, &boundX, &boundY, &boundW, &boundH);
    display.setCursor((SCREEN_WIDTH - boundW) / 2, SCREEN_HEIGHT - boundH - 1);
    display.print(pageDisplayName);

    display.display();
}   
