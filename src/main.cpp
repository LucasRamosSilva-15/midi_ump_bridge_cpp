#include <iostream>
#include <iomanip>
#include "converter.h"
#include "ump.h"
#include "gui.h"

int main() {
    std::cout << "\n--- Iniciando Teste de Conversao C++ (Centro Perfeito) ---\n";
    
    uint16_t val_midi1 = 0;
    uint32_t p32 = midi1_to_midi2_pitch(val_midi1);
    UMPMessage msg = create_midi2_pitch_bend(p32);
    
    std::cout << "Entrada (Mido simulado): " << val_midi1 << " (14-bit)\n";
    std::cout << "Saida UMP convertida: " << p32 << " (32-bit)\n";
    std::cout << "UMP HEX (Espera-se 80000000 no Word 2): 0x" 
              << std::hex << std::uppercase << msg.word2 << "\n";

    return 0;
}