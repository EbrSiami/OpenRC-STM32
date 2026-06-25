/**
 * @file Settings.h
 * @author Александр Королёв
 * @brief EEPROM Data Structure Definition
 * @version 4.0.1
 * @date 2026-04-13
 *
 * Description:
 * Defines the 'RadioSettings' structure used to persist configuration data
 * (trims, channel inversions, UI preferences) into the external EEPROM.
 * 
 * WARNING: Changing the order or type of members in this struct will
 * invalidate existing EEPROM data!
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

struct RadioSettings {

    uint32_t magic;

    // --- Trim Configuration ---
    // Range: 0 - 4095 (Center: 2048)
    int trim1;                // Channel 1 (Roll/Aileron)
    int trim2;                // Channel 2 (Pitch/Elevator)
    int trim3;                // Channel 3 (Yaw/Rudder)

    // --- Calibration Configuration ---
    // 4 main channels (1-roll, 2-elevator, 3-throttle, 4-rudder)
    int calibMin[4];
    int calibCenter[4];
    int calibMax[4];
    
    // --- Advanced Channel Config (EPA & Subtrim) ---
    int epaMin[4];
    int subTrim[4];
    int epaMax[4];

    // --- Expo Configuration ---
    int expoRoll;
    int expoPitch;
    int expoYaw;

    // --- UI Preferences ---
    bool buzzerEnabled;       // true = Sound On, false = Mute
    bool lightModeEnabled;    // true = Light Background, false = Dark Background
    
    // --- Channel Logic ---
    bool channelInverted[8];  // Inversion map for CH1-CH8 (true = Inverted)

    // --- Flight Mode Configuration ---
    // false = Normal/Quad Mode (Standard linear mapping)
    // true  = Airplane Mode (map upper side of stick to whole 0-4096)
    bool airplaneMode; 

    // --- Dual Rate Configuration ---
    // Range: 0 to 100 (Percentage). 100 = full travel, 50 = half travel.
    bool dualRateEnabled;
    uint8_t dualRateRoll;   // CH1
    uint8_t dualRatePitch;  // CH2
    uint8_t dualRateYaw;    // CH4

    // --- Channels Mix Mode ---
    uint8_t mixMode;

    // --- Data Integrity ---
    uint8_t checksum;
};

#endif // SETTINGS_H