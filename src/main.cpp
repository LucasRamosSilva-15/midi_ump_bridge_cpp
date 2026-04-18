#include <iostream>
#include <QApplication>
#include <RtMidi.h>
#include "gui.h"
#include "converter.h"

void test_pitch_bend() {
    std::cout << "\n--- Iniciando Teste de Conversao (Centro Perfeito) ---\n";
    uint16_t val_midi1 = 0;
    uint32_t p32 = midi1_to_midi2_pitch(val_midi1);
    UMPMessage ump = create_midi2_pitch_bend(p32);
    
    std::cout << "Entrada Mido: " << val_midi1 << " (14-bit)\n";
    std::cout << "Saida UMP: " << p32 << " (32-bit)\n";
    std::cout << "UMP HEX (Espera-se final 80000000): 0x" << std::hex << ump.ump64() << std::dec << "\n";
}

RtMidiIn* select_midi_input() {
    RtMidiIn* midiin = nullptr;
    try { midiin = new RtMidiIn(); } catch (RtMidiError &error) { return nullptr; }

    unsigned int nPorts = midiin->getPortCount();
    if (nPorts == 0) {
        std::cout << "Nenhum dispositivo MIDI encontrado.\n";
        delete midiin;
        return nullptr;
    }

    std::cout << "\n--- Entradas Disponiveis ---\n";
    for (unsigned int i = 0; i < nPorts; i++) {
        std::cout << i << ": " << midiin->getPortName(i) << '\n';
    }

    std::cout << "Escolha o numero da porta: ";
    unsigned int port;
    if (!(std::cin >> port) || port >= nPorts) {
        std::cout << "Selecao invalida.\n";
        delete midiin;
        return nullptr;
    }

    midiin->openPort(port);
    return midiin;
}

int main(int argc, char *argv[]) {
    RtMidiIn* port = select_midi_input();
    if (!port) {
        std::cout << "Nenhum dispositivo selecionado. Saindo...\n";
        return 0;
    }

    std::cout << "Rodar teste de unidade? (s/n)\n";
    char choice;
    std::cin >> choice;
    if (choice == 's') test_pitch_bend();

    QApplication app(argc, argv);
    MainWindow win(port);
    win.show();
    return app.exec();
}