/**
 * @file Button.h
 * @author Александр Королёв
 * @brief Button Class Interface
 * @version 4.0.1
 * @date 2026-04-08
 */

#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>

class Button {
public:
    /**
     * @brief Construct a new Button object.
     * 
     * @param pin The STM32 GPIO pin connected to the button.
     * @param debounceDelay Time in ms to wait for signal stabilization (default: 50ms).
     */
    Button(int pin, long debounceDelay = 50);

    /**
     * @brief Configures the pin as INPUT_PULLUP.
     * Must be called in setup() before using the button.
     */
    void begin();

    /**
     * @brief Reads the physical button state and updates internal logic.
     * CRITICAL: Must be called continuously in the main loop() (polling).
     */
    void update();

    /**
     * @brief Checks if a "Click" event occurred (Press followed by Release).
     * This flag is cleared after reading (consume-on-read).
     * 
     * @return true if the button was just released.
     */
    bool wasJustPressed();

    /**
     * @brief Checks the real-time debounced state.
     * 
     * @return true while the button is physically held down.
     */
    bool isBeingHeld();
    
    /**
     * @brief Checks if the button is held down, repeating the action.
     * @param startDelayMs Wait time before repeating starts (default 500ms).
     * @param repeatDelayMs Speed of the repeat ticks (default 150ms).
     * @return true on every repeat tick.
     */
    bool isAutoRepeating(unsigned long startDelayMs = 500, unsigned long repeatDelayMs = 150);

private:
    int _pin;                       // Hardware pin number
    long _debounceDelay;            // Noise filter duration in ms
    
    bool _lastPhysicalState;        // Immediate reading from the previous loop
    bool _buttonIsCurrentlyPressed; // Stable, debounced state (True = Held)
    bool _wasJustPressedFlag;       // Event flag for single click detection
    
    unsigned long _lastDebounceTime; // Timestamp of the last state change

    unsigned long _pressStartTime;   // When the button was first pressed
    unsigned long _lastRepeatTime;   // Time of the last auto-repeat tick
    bool _handledAsHold;             // Prevents release-click if it was held
};

#endif // BUTTON_H