/**
 * @file Settings.h
 * @author Александр Королёв
 * @brief Simulator Data Protocol
 * @version 4.0.1
 * @date 2026-04-27
 */

#pragma once
#include <Arduino.h>

namespace SimProto {

#pragma pack(push, 1)
struct __attribute__((packed)) Packet {
    uint8_t  header1 = 0xAA;
    uint8_t  header2 = 0xBB;
    uint8_t  version = 1;
    uint8_t  seq     = 0;
    uint16_t channels[6];   // 12-bit values
    uint8_t  auxSwitches;
    uint8_t  crc;
};
#pragma pack(pop)

static_assert(sizeof(Packet) == 18, "Packet size mismatch");

uint8_t crc8(const uint8_t* data, size_t len);

void send(
    uint16_t r, uint16_t p, uint16_t t, uint16_t y, 
    uint16_t a1, uint16_t a2, bool a3, bool a4
    );

} // namespace SimProto