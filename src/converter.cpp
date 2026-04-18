#include "converter.h"

uint32_t midi1_to_midi2_velocity(uint8_t v) {
    if (v == 0) return 0;
    return (uint32_t)((v << 9) | (v << 2) | (v >> 5));
}

uint32_t midi1_to_midi2_pitch(uint16_t pitch14) {
    return (uint32_t)pitch14 << 18;
}

uint32_t midi1_to_midi2_32bit(uint8_t value7) {
    return (uint32_t)((value7 / 127.0) * 0xFFFFFFFF);
}