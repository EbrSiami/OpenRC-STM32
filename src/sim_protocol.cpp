/**
 * @file Settings.h
 * @author Александр Королёв
 * @brief Simulator Data Protocol
 * @version 4.0.1
 * @date 2026-04-27
 */

#include "sim_protocol.h"

namespace SimProto {

uint8_t crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0x00;
    while (len--) {
        crc ^= *data++;
        for (uint8_t i = 0; i < 8; i++)
            crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : (crc << 1);
    }
    return crc;
}

void send(uint16_t r, uint16_t p, uint16_t t, uint16_t y, uint16_t a1, uint16_t a2, bool a3, bool a4) {
    static uint8_t seq = 0;
    Packet packet;

    packet.seq = seq++;

    packet.channels[0] = r  & 0x0FFF;
    packet.channels[1] = p  & 0x0FFF;
    packet.channels[2] = t  & 0x0FFF;
    packet.channels[3] = y  & 0x0FFF;
    packet.channels[4] = a1 & 0x0FFF;
    packet.channels[5] = a2 & 0x0FFF;

    packet.auxSwitches = 0;
    if (a3) packet.auxSwitches |= 1 << 0;
    if (a4) packet.auxSwitches |= 1 << 1;

    packet.crc = crc8((uint8_t*)&packet, sizeof(Packet) - 1);

    Serial.write((uint8_t*)&packet, sizeof(Packet));
}

} // namespace SimProto