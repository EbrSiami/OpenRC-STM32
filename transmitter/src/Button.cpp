/**
 * @file Button.cpp
 * @author Александр Королёв
 * @brief Non-blocking Switch Debouncing Implementation
 * @version 4.0.1
 * @date 2026-04-08
 */

#include "Button.h"
#include <Arduino.h>

/**
 * @brief Construct a new Button object.
 * 
 * @param pin The GPIO pin number connected to the button.
 * @param debounceDelay The stabilization time in milliseconds (e.g., 50ms).
 */
Button::Button(int pin, long debounceDelay)
    : _pin(pin),
      _debounceDelay(debounceDelay),
      _lastPhysicalState(HIGH),
      _buttonIsCurrentlyPressed(false),
      _wasJustPressedFlag(false),
      _lastDebounceTime(0),
      _pressStartTime(0),
      _lastRepeatTime(0),
      _handledAsHold(false) {
}

/**
 * @brief Initializes the hardware pin.
 * Must be called in setup().
 */
void Button::begin() {
    pinMode(_pin, INPUT_PULLUP);
}

/**
 * @brief Reads the button state and manages debouncing logic.
 * Must be called frequently in the main loop.
 */
void Button::update() {
    bool reading = digitalRead(_pin);
    unsigned long currentTime = millis();

    // 1. Detect noise or state change
    // If the switch changed, due to noise or pressing:
    if (reading != _lastPhysicalState) {
        _lastDebounceTime = currentTime;
        _lastPhysicalState = reading;
    }

    // 2. Check if state is stable
    if ((currentTime - _lastDebounceTime) > _debounceDelay) {
        
        // Logic for Active-Low (Low = Pressed, High = Released)
        
        // Case A: Button pressed
        if (reading == LOW && !_buttonIsCurrentlyPressed) {
            _buttonIsCurrentlyPressed = true;
            _pressStartTime = currentTime; // Record start time
            _lastRepeatTime = currentTime; 
            _handledAsHold = false;        // Reset hold flag
        }
        
        // Case B: Button released
        else if (reading == HIGH && _buttonIsCurrentlyPressed) {
            _buttonIsCurrentlyPressed = false;
            // Only trigger click IF it wasn't held down for Auto-Repeat
            if (!_handledAsHold) {
                _wasJustPressedFlag = true; 
            }
        }
    }
}

/**
 * @brief Checks if the button was clicked since the last check.
 * This flag is cleared after reading (consume-on-read).
 * 
 * @return true if button was pressed and released.
 * @return false otherwise.
 */
bool Button::wasJustPressed() {
    if (_wasJustPressedFlag) {
        _wasJustPressedFlag = false; // Reset flag after reading
        return true;
    }
    return false;
}

/**
 * @brief Checks the current real-time state of the button.
 * Useful for continuous actions (like holding trim buttons).
 * 
 * @return true if button is currently held down.
 */
bool Button::isBeingHeld() {
    return _buttonIsCurrentlyPressed;
}

bool Button::isAutoRepeating(unsigned long startDelayMs, unsigned long repeatDelayMs) {
    if (_buttonIsCurrentlyPressed) {
        unsigned long currentTime = millis();
        // Has it been held long enough to start repeating?
        if ((currentTime - _pressStartTime) >= startDelayMs) {
            // Is it time for the next tick?
            if ((currentTime - _lastRepeatTime) >= repeatDelayMs) {
            _lastRepeatTime = currentTime;
                _handledAsHold = true; // Mark as held (cancels the release click!)
                return true;
            }
        }
    }
    return false;
}
