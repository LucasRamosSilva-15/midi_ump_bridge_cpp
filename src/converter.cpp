#include "converter.h"

uint32_t midi1_to_midi2_velocity(uint8_t v) {
    return (uint32_t)((v / 127.0) * 65535.0);
}

uint32_t midi1_to_midi2_pitch(uint16_t pitch14) {
    uint32_t pitch14_unsigned = pitch14 + 8192;
    return pitch14_unsigned << 18;
}

uint32_t midi1_to_midi2_32bit(uint8_t value7) {
    return (uint32_t)((value7 / 127.0) * 0xFFFFFFFF);
}