#include <iostream>
#include <iomanip>
#include <cmath>
#include <atomic>
#include <vector>
#include <string>
#include <csignal>
#include <chrono>
#include <thread>

#define MINIAUDIO_IMPLEMENTATION
#include "../third_party/miniaudio.h"

#include <RtMidi.h>
#include "ump.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct SynthState {
    std::atomic<bool> note_active{false};
    std::atomic<uint8_t> current_note{60};
    std::atomic<double> target_freq{261.63};
    std::atomic<double> amplitude{0.0};
    std::atomic<double> target_amp{0.0};
    std::atomic<double> pitch_offset_semitones{0.0};
    std::atomic<double> mod_depth{0.0};
    double phase{0.0};
    double lfo_phase{0.0};
};

static SynthState g_synth;
static std::atomic<bool> g_running{true};

void signal_handler(int) {
    g_running = false;
}

void audio_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    (void)pInput;
    float* pOut = (float*)pOutput;
    double sampleRate = (double)pDevice->sampleRate;
    const double two_pi = 2.0 * M_PI;
    const double lfo_freq = 5.5;
    const double lfo_inc = two_pi * lfo_freq / sampleRate;

    for (ma_uint32 i = 0; i < frameCount; ++i) {
        double cur_amp = g_synth.amplitude.load();
        double tgt_amp = g_synth.target_amp.load();
        cur_amp += (tgt_amp - cur_amp) * 0.005;
        g_synth.amplitude.store(cur_amp);

        double base_f = g_synth.target_freq.load();
        double pitch_st = g_synth.pitch_offset_semitones.load();

        g_synth.lfo_phase += lfo_inc;
        if (g_synth.lfo_phase >= two_pi) g_synth.lfo_phase -= two_pi;

        double lfo_val = std::sin(g_synth.lfo_phase);
        double depth = g_synth.mod_depth.load();

        double mod_st = lfo_val * (depth * 2.5);
        double total_st = pitch_st + mod_st;
        double effective_freq = base_f * std::pow(2.0, total_st / 12.0);

        double phase_inc = two_pi * effective_freq / sampleRate;
        g_synth.phase += phase_inc;
        if (g_synth.phase >= two_pi) g_synth.phase -= two_pi;

        double beta = depth * 1.8;
        double modulated_phase = g_synth.phase + beta * std::sin(g_synth.phase);
        double tremolo = 1.0 - (depth * 0.25 * (1.0 + lfo_val) * 0.5);

        float sample = (float)(std::sin(modulated_phase) * cur_amp * tremolo);
        pOut[i * 2 + 0] = sample;
        pOut[i * 2 + 1] = sample;
    }
}

void midi_callback(double timeStamp, std::vector<unsigned char>* message, void* userData) {
    (void)timeStamp;
    (void)userData;
    if (!message || message->empty()) return;

    UMPMessage ump(0, 0);
    bool is_ump = false;

    if (UMPMessage::fromSysEx(*message, ump)) {
        is_ump = true;
    } else if (message->size() >= 3) {
        uint8_t status = (*message)[0] >> 4;
        uint8_t ch = (*message)[0] & 0x0F;
        uint8_t d1 = (*message)[1];
        uint8_t d2 = (*message)[2];
        if (status == 0x9) ump = create_midi2_note_on(d1, (d2 << 9) | (d2 << 2), ch);
        else if (status == 0x8) ump = create_midi2_note_off(d1, (d2 << 9) | (d2 << 2), ch);
        else if (status == 0xE) ump = create_midi2_pitch_bend(((d2 << 7) | d1) << 18, ch);
        else if (status == 0xB) ump = create_midi2_control_change(d1, d2 << 25, ch);
        is_ump = true;
    }

    if (!is_ump) return;

    uint8_t mt = (ump.word1 >> 28) & 0x0F;
    uint8_t status = (ump.word1 >> 20) & 0x0F;
    uint8_t note_idx = (ump.word1 >> 8) & 0xFF;

    if (mt == 4) {
        if (status == 0x9) {
            uint16_t vel16 = (ump.word2 >> 16) & 0xFFFF;
            if (vel16 > 0) {
                double f = 440.0 * std::pow(2.0, ((int)note_idx - 69.0) / 12.0);
                g_synth.current_note.store(note_idx);
                g_synth.target_freq.store(f);
                g_synth.target_amp.store((double)vel16 / 65535.0 * 0.4);
                g_synth.note_active.store(true);

                std::cout << "\033[1;32m[NOTE ON]\033[0m "
                          << "Nota: " << std::setw(3) << (int)note_idx
                          << " | Vel(16-bit): " << std::setw(5) << vel16
                          << " | Freq: " << std::fixed << std::setprecision(2) << f << " Hz"
                          << " | UMP: 0x" << std::hex << std::setw(8) << std::setfill('0') << ump.word1
                          << " 0x" << std::setw(8) << std::setfill('0') << ump.word2 << std::dec << std::endl;
            } else {
                g_synth.target_amp.store(0.0);
                g_synth.note_active.store(false);
                std::cout << "\033[1;33m[NOTE OFF (Vel 0)]\033[0m Nota: " << (int)note_idx << std::endl;
            }
        }
        else if (status == 0x8) {
            if (g_synth.current_note.load() == note_idx) {
                g_synth.target_amp.store(0.0);
                g_synth.note_active.store(false);
            }
            std::cout << "\033[1;33m[NOTE OFF]\033[0m Nota: " << (int)note_idx << std::endl;
        }
        else if (status == 0xE) {
            uint32_t pitch32 = ump.word2;
            double norm = ((double)pitch32 - 2147483648.0) / 2147483648.0;
            double semitones = norm * 2.0;
            g_synth.pitch_offset_semitones.store(semitones);

            double cur_base = g_synth.target_freq.load();
            double modulated_f = cur_base * std::pow(2.0, semitones / 12.0);

            int bar_w = 20;
            int pos = (int)((norm + 1.0) * 0.5 * bar_w);
            if (pos < 0) pos = 0; if (pos >= bar_w) pos = bar_w - 1;
            std::string meter(bar_w, '.');
            meter[bar_w / 2] = '|';
            meter[pos] = '#';

            std::cout << "\033[1;36m[PITCH BEND (32-bit)]\033[0m "
                      << "Val: 0x" << std::hex << std::setw(8) << std::setfill('0') << pitch32 << std::dec
                      << " | Offset: " << std::showpos << std::fixed << std::setprecision(2) << semitones << " st"
                      << " | Freq Modulada: " << std::noshowpos << std::setprecision(2) << modulated_f << " Hz "
                      << "[" << meter << "]" << std::endl;
        }
        else if (status == 0xB) {
            uint8_t index = (ump.word1 >> 8) & 0xFF;
            if (index == 1) {
                uint32_t val32 = ump.word2;
                double depth = (double)val32 / 4294967295.0;
                g_synth.mod_depth.store(depth);

                std::cout << "\033[1;35m[MOD WHEEL (CC1)]\033[0m "
                          << "Val: 0x" << std::hex << std::setw(8) << std::setfill('0') << val32 << std::dec
                          << " | Vibrato LFO: " << std::fixed << std::setprecision(1) << (depth * 100.0) << "%" << std::endl;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    std::cout << "=========================================================\n";
    std::cout << "  UMP Audio Sink - Sintetizador Receptor MIDI 2.0 (TCC)  \n";
    std::cout << "=========================================================\n";

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate        = 48000;
    config.dataCallback      = audio_callback;
    config.pUserData         = nullptr;

    ma_device device;
    if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS) {
        std::cerr << "ERRO: Falha ao inicializar dispositivo de audio!\n";
        return 1;
    }

    if (ma_device_start(&device) != MA_SUCCESS) {
        std::cerr << "ERRO: Falha ao iniciar reproducao de audio!\n";
        ma_device_uninit(&device);
        return 1;
    }

    std::cout << "[Audio] Dispositivo ativo: " << device.playback.name << " (48000 Hz, Stereo)\n";

    RtMidiIn* midiIn = nullptr;
    try {
        midiIn = new RtMidiIn();
    } catch (RtMidiError &error) {
        std::cerr << "ERRO RtMidi: " << error.getMessage() << "\n";
        ma_device_uninit(&device);
        return 1;
    }

    unsigned int portCount = midiIn->getPortCount();
    std::cout << "[MIDI] Portas de entrada detectadas: " << portCount << "\n";
    for (unsigned int i = 0; i < portCount; ++i) {
        try {
            std::cout << "   [" << i << "] " << midiIn->getPortName(i) << "\n";
        } catch (...) {}
    }

    int portToOpen = -1;
    if (argc > 1) {
        portToOpen = std::stoi(argv[1]);
    }

    if (portToOpen >= 0 && (unsigned int)portToOpen < portCount) {
        std::cout << "[MIDI] Conectando a porta existente [" << portToOpen << "]: "
                  << midiIn->getPortName(portToOpen) << "\n";
        midiIn->openPort(portToOpen);
    } else {
        std::cout << "[MIDI] Criando porta virtual: \"UMP Audio Sink Input\"...\n";
        try {
            midiIn->openVirtualPort("UMP Audio Sink Input");
            std::cout << "[MIDI] Porta virtual criada com sucesso! Selecione \"UMP Audio Sink Input\" no MIDI2Bridge.\n";
        } catch (RtMidiError &e) {
            if (portCount > 0) {
                std::cout << "[MIDI] Abrindo primeira porta disponivel [0]...\n";
                midiIn->openPort(0);
            } else {
                std::cerr << "[MIDI] Nenhuma porta disponivel: " << e.getMessage() << "\n";
            }
        }
    }

    midiIn->setCallback(&midi_callback);
    midiIn->ignoreTypes(false, false, false);

    std::cout << "\n---------------------------------------------------------\n";
    std::cout << "Pronto! Aguardando mensagens UMP (Notas, Pitch Bend, CC1)...\n";
    std::cout << "Pressione Ctrl+C para encerrar.\n";
    std::cout << "---------------------------------------------------------\n\n";

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\nFinalizando subsistemas...\n";
    midiIn->closePort();
    delete midiIn;

    ma_device_stop(&device);
    ma_device_uninit(&device);

    std::cout << "Encerrado com sucesso.\n";
    return 0;
}
