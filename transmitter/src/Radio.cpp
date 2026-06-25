/**
 * @file Radio.cpp
 * @author Александр Королёв
 * @brief NRF24L01+ Communication Driver
 * @version 4.0.1
 * @date 2026-04-23
 *
 * Description:
 * Handles initialization and data transmission using the RF24 library.
 * Configured for long-range, low-latency RC control.
 */

#include "Radio.h"
#include <SPI.h>

// =============================================================================
// --- Configuration & Globals ---
// =============================================================================

bool radioIsOK = false;

// Initialize RF24 Object (CE Pin, CSN Pin defined in Radio.h)
RF24 radio(RF_CE_PIN, RF_CSN_PIN);

// Radio Pipe Address
// WARNING: This must strictly match the address defined in the Receiver firmware.
const uint64_t pipeOut = 0xE8E8F0F0E1LL;

// =============================================================================
// --- Functions ---
// =============================================================================

/**
 * @brief Initializes the NRF24L01 module.
 *
 * Settings:
 * - Channel: 100 (2.500 GHz - avoids most WiFi interference).
 * - Data Rate: 250kbps (Offers maximum receiver sensitivity/range).
 * - PA Level: MAX (Maximum transmission power).
 * - AutoAck: Disabled (Provides fixed latency, similar to UDP).
 */
void setupRadio() {
    SPI.begin();

    if (radio.begin()) {
        radioIsOK = true; // Chip is alive!
    } else {
        radioIsOK = false; // Chip is dead or disconnected!
    }

    radio.openWritingPipe(pipeOut);
    radio.setChannel(100);
    radio.setAutoAck(false);           // Disable ACK for consistent loop time
    radio.setDataRate(RF24_250KBPS);   // Best range
    radio.setPALevel(RF24_PA_MAX);     // Max power
    radio.stopListening();             // Ensure Transmitter Mode
}

/**
 * @brief Transmits the control data packet over the air.
 * 
 * @param dataToSend The structured data packet containing channel values.
 */
void sendRadioData(data_t dataToSend) {
    radio.write(&dataToSend, sizeof(data_t));
}

bool getRadioStatus() {
    return radioIsOK;
}

void setRadioPower(bool enable) {
    if (enable) {
        radio.powerUp(); 
        delay(500);
        radio.stopListening();
    } else {
        radio.powerDown();
    }
}
