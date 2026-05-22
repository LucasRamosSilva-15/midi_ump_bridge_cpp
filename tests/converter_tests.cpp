#include "converter.h"

#include <cassert>
#include <cstdint>

int main() {
    assert(midi1_to_midi2_velocity(0) == 0);
    assert(midi1_to_midi2_velocity(1) == 516);
    assert(midi1_to_midi2_velocity(64) == 33026);
    assert(midi1_to_midi2_velocity(127) == 65535);

    assert(midi1_to_midi2_pitch(0) == 0x00000000u);
    assert(midi1_to_midi2_pitch(8192) == 0x80000000u);
    assert(midi1_to_midi2_pitch(16383) == 0xFFFC0000u);

    assert(midi1_to_midi2_32bit(0) == 0x00000000u);
    assert(midi1_to_midi2_32bit(127) == 0xFFFFFFFFu);

    return 0;
}
