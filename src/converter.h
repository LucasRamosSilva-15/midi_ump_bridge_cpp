#ifndef CONVERTER_H
#define CONVERTER_H

#include <cstdint>

uint32_t midi1_to_midi2_velocity(uint8_t v);
uint32_t midi1_to_midi2_pitch(uint16_t pitch14);
uint32_t midi1_to_midi2_32bit(uint8_t value7);

#endif // CONVERTER_H