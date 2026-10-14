#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QProgressBar>
#include <QStatusBar>
#include <QTimer>
#include <QKeyEvent>
#include <QRadioButton>
#include <QButtonGroup>
#include <QFrame>

#include <iostream>
#include <iomanip>
#include <cmath>
#include <atomic>
#include <vector>
#include <string>

#define MINIAUDIO_IMPLEMENTATION
#include "../third_party/miniaudio.h"

#include <RtMidi.h>
#include "ump.h"
#include "oscilloscope.h"

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
    std::atomic<double> filter_cutoff_norm{1.0}; // 0.0 a 1.0 (CC74 Cutoff)
    std::atomic<bool> midi2_mode{true};          // true = MIDI 2.0 (32-bit), false = MIDI 1.0 (7-bit)
    std::atomic<uint32_t> last_pitch32{0x80000000};
    std::atomic<uint16_t> last_vel16{0};
    std::atomic<double> instant_freq{261.63};

    double phase{0.0};
    double lfo_phase{0.0};
    float filter_prev{0.0f};
};

static SynthState g_synth;
static WaveformDisplay* g_display = nullptr;

void audio_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    (void)pInput;
    if (!pOutput || frameCount == 0) return;
    float* pOut = (float*)pOutput;
    double sampleRate = (double)pDevice->sampleRate;
    if (sampleRate <= 0.0) sampleRate = 48000.0;
    const double two_pi = 2.0 * M_PI;
    const double lfo_freq = 5.5;
    const double lfo_inc = two_pi * lfo_freq / sampleRate;

    std::vector<float> monoSamples(frameCount, 0.0f);

    for (ma_uint32 i = 0; i < frameCount; ++i) {
        double cur_amp = g_synth.amplitude.load();
        double tgt_amp = g_synth.target_amp.load();
        if (std::isnan(cur_amp)) cur_amp = 0.0;
        if (std::isnan(tgt_amp)) tgt_amp = 0.0;
        cur_amp += (tgt_amp - cur_amp) * 0.005;
        g_synth.amplitude.store(cur_amp);

        double base_f = g_synth.target_freq.load();
        if (base_f <= 0.0 || std::isnan(base_f) || std::isinf(base_f)) base_f = 261.63;

        double pitch_st = g_synth.pitch_offset_semitones.load();
        if (std::isnan(pitch_st) || std::isinf(pitch_st)) pitch_st = 0.0;

        g_synth.lfo_phase += lfo_inc;
        if (g_synth.lfo_phase >= two_pi) g_synth.lfo_phase -= two_pi;

        double lfo_val = std::sin(g_synth.lfo_phase);
        double depth = g_synth.mod_depth.load();
        if (depth < 0.0 || std::isnan(depth)) depth = 0.0;
        else if (depth > 1.0) depth = 1.0;

        // 1. Modulação de Frequência LFO (Vibrato acústico ±2.5 semitons)
        double mod_st = lfo_val * (depth * 2.5);
        double total_st = pitch_st + mod_st;
        double effective_freq = base_f * std::pow(2.0, total_st / 12.0);
        if (effective_freq < 10.0 || std::isnan(effective_freq)) effective_freq = 10.0;
        if (effective_freq > sampleRate * 0.45) effective_freq = sampleRate * 0.45;
        g_synth.instant_freq.store(effective_freq);

        double phase_inc = two_pi * effective_freq / sampleRate;
        g_synth.phase += phase_inc;
        if (g_synth.phase >= two_pi) g_synth.phase -= two_pi;

        // 2. Modulação Harmônica de Fase / FM (Deformação visual da forma de onda para comparativo 4.3)
        double beta = depth * 1.8;
        double modulated_phase = g_synth.phase + beta * std::sin(g_synth.phase);

        // 3. Modulação de Amplitude (Tremolo síncrono suave)
        double tremolo = 1.0 - (depth * 0.25 * (1.0 + lfo_val) * 0.5);

        // Onda base com harmônicos ricos para demonstração de filtro passa-baixas e ruído de zíper
        double fundamental = std::sin(modulated_phase);
        double h2 = 0.30 * std::sin(modulated_phase * 2.0);
        double h3 = 0.15 * std::sin(modulated_phase * 3.0);
        double h4 = 0.08 * std::sin(modulated_phase * 4.0);
        double raw_wave = (fundamental + h2 + h3 + h4) * 0.65;

        // 4. Filtro Passa-Baixas CC74 (Demonstrador clássico do ruído de zíper / Zipper Noise)
        double cutoff_norm = g_synth.filter_cutoff_norm.load();
        if (cutoff_norm < 0.005) cutoff_norm = 0.005;
        else if (cutoff_norm > 1.0) cutoff_norm = 1.0;

        // Mapeamento exponencial de 80 Hz a 18000 Hz
        double fc = 80.0 * std::pow(18000.0 / 80.0, cutoff_norm);
        double alpha = 1.0 - std::exp(-two_pi * fc / sampleRate);
        if (alpha > 1.0) alpha = 1.0;
        else if (alpha < 0.001) alpha = 0.001;

        g_synth.filter_prev += (float)(alpha * ((raw_wave * cur_amp * tremolo) - g_synth.filter_prev));
        if (std::isnan(g_synth.filter_prev) || std::isinf(g_synth.filter_prev)) g_synth.filter_prev = 0.0f;
        float sample = g_synth.filter_prev;

        if (sample > 1.0f) sample = 1.0f;
        else if (sample < -1.0f) sample = -1.0f;

        pOut[i * 2 + 0] = sample;
        pOut[i * 2 + 1] = sample;
        monoSamples[i] = sample;
    }

    if (g_display) {
        g_display->pushSamples(monoSamples.data(), frameCount);
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
        else if (status == 0xB) {
            uint32_t val32 = (uint32_t)((d2 / 127.0) * 0xFFFFFFFF);
            ump = create_midi2_control_change(d1, val32, ch);
        }
        is_ump = true;
    }

    if (!is_ump) return;

    uint8_t mt = (ump.word1 >> 28) & 0x0F;
    uint8_t status = (ump.word1 >> 20) & 0x0F;
    uint8_t note_idx = (ump.word1 >> 8) & 0xFF;

    if (mt == 4) {
        if (status == 0x9) {
            uint16_t vel16 = (ump.word2 >> 16) & 0xFFFF;
            g_synth.last_vel16.store(vel16);
            if (vel16 > 0) {
                double f = 440.0 * std::pow(2.0, ((int)note_idx - 69.0) / 12.0);
                g_synth.current_note.store(note_idx);
                g_synth.target_freq.store(f);
                g_synth.target_amp.store((double)vel16 / 65535.0 * 0.4);
                g_synth.note_active.store(true);
            } else {
                g_synth.target_amp.store(0.0);
                g_synth.note_active.store(false);
            }
        }
        else if (status == 0x8) {
            if (g_synth.current_note.load() == note_idx) {
                g_synth.target_amp.store(0.0);
                g_synth.note_active.store(false);
            }
        }
        else if (status == 0xE) {
            uint32_t pitch32 = ump.word2;
            g_synth.last_pitch32.store(pitch32);
            double norm = ((double)pitch32 - 2147483648.0) / 2147483648.0;
            g_synth.pitch_offset_semitones.store(norm * 2.0);
        }
        else if (status == 0xB) {
            uint8_t index = (ump.word1 >> 8) & 0xFF;
            if (index == 1) {
                uint32_t val32 = ump.word2;
                g_synth.mod_depth.store((double)val32 / 4294967295.0);
            } else if (index == 74) {
                uint32_t val32 = ump.word2;
                g_synth.filter_cutoff_norm.store((double)val32 / 4294967295.0);
            }
        }
    }
    else if (mt == 2) {
        if (status == 0x9) {
            uint8_t vel7 = ump.word1 & 0x7F;
            if (vel7 > 0) {
                double f = 440.0 * std::pow(2.0, ((int)note_idx - 69.0) / 12.0);
                g_synth.current_note.store(note_idx);
                g_synth.target_freq.store(f);
                g_synth.target_amp.store((double)vel7 / 127.0 * 0.4);
                g_synth.note_active.store(true);
            } else {
                g_synth.target_amp.store(0.0);
                g_synth.note_active.store(false);
            }
        }
        else if (status == 0x8) {
            if (g_synth.current_note.load() == note_idx) {
                g_synth.target_amp.store(0.0);
                g_synth.note_active.store(false);
            }
        }
        else if (status == 0xB) {
            uint8_t index = (ump.word1 >> 8) & 0xFF;
            if (index == 1) {
                uint8_t val7 = ump.word1 & 0x7F;
                g_synth.mod_depth.store((double)val7 / 127.0);
            } else if (index == 74) {
                uint8_t val7 = ump.word1 & 0x7F;
                g_synth.filter_cutoff_norm.store((double)val7 / 127.0);
            }
        }
    }
}

class OscilloscopeWindow : public QMainWindow {
public:
    OscilloscopeWindow(RtMidiIn* midi, bool portOpened, const QString& portName, QWidget *parent = nullptr)
        : QMainWindow(parent), m_midi(midi), m_portOpened(portOpened), m_portName(portName), m_refFreq(0.0),
          m_sweeping(false), m_sweepPhase(0.0)
    {
        setWindowTitle("UMP Audio Oscilloscope - Demonstração de Resolução MIDI 2.0 vs Zipper Noise (TCC 4.3)");
        resize(1040, 840);
        setMinimumSize(960, 760);
        setStyleSheet(
            "QMainWindow { background-color: #10151a; color: #e0f2f1; }"
            "QLabel { color: #b2dfdb; font-family: monospace; }"
            "QGroupBox { font-weight: bold; border: 1px solid #004d40; border-radius: 6px; margin-top: 10px; padding: 10px; background-color: #080d10; color: #80cbc4; }"
            "QGroupBox::title { subcontrol-origin: margin; left: 14px; padding: 0 4px; }"
            "QProgressBar { border: 1px solid #004d40; border-radius: 4px; text-align: center; background-color: #050b0e; color: #e0f2f1; }"
            "QProgressBar::chunk { background-color: #00e5ff; }"
            "QPushButton { background-color: #004d40; color: #e0f2f1; border: 1px solid #00bfa5; border-radius: 4px; padding: 6px 14px; font-weight: bold; font-family: sans-serif; }"
            "QPushButton:hover { background-color: #00695c; }"
            "QPushButton:pressed { background-color: #00897b; }"
            "QRadioButton { color: #e0f2f1; font-weight: bold; font-family: sans-serif; spacing: 8px; }"
            "QRadioButton::indicator { width: 14px; height: 14px; border-radius: 7px; border: 2px solid #00bfa5; background-color: #050b0e; }"
            "QRadioButton::indicator:checked { background-color: #00ffaa; border-color: #00ffaa; }"
        );

        QWidget* central = new QWidget(this);
        QVBoxLayout* mainLayout = new QVBoxLayout(central);
        mainLayout->setContentsMargins(14, 14, 14, 14);
        mainLayout->setSpacing(10);

        // --- BARRA SUPERIOR (STATUS E CONTROLES DE REFERÊNCIA) ---
        QHBoxLayout* headerLayout = new QHBoxLayout();
        lblStatus = new QLabel(this);
        if (m_portOpened) {
            lblStatus->setText(QString("● CONECTADO: %1").arg(m_portName));
            lblStatus->setStyleSheet("font-size: 13px; font-weight: bold; color: #00ffaa;");
        } else {
            lblStatus->setText("○ ENTRADA MIDI (PROCURANDO PORTAS...)");
            lblStatus->setStyleSheet("font-size: 13px; font-weight: bold; color: #f59e0b;");
        }

        btnFreezeRef = new QPushButton("📌 Fixar Som Original (Referência)", this);
        btnFreezeRef->setToolTip("Congela a onda do som original como linha azul para comparacao direta com o som modulado");
        connect(btnFreezeRef, &QPushButton::clicked, this, &OscilloscopeWindow::onCaptureReference);

        btnClearRef = new QPushButton("✕ Limpar Referência", this);
        connect(btnClearRef, &QPushButton::clicked, this, &OscilloscopeWindow::onClearReference);

        lblNote = new QLabel("NOTA: NENHUMA", this);
        lblNote->setStyleSheet("font-size: 13px; font-weight: bold; color: #80cbc4;");

        headerLayout->addWidget(lblStatus);
        headerLayout->addSpacing(12);
        headerLayout->addWidget(btnFreezeRef);
        headerLayout->addWidget(btnClearRef);
        headerLayout->addStretch();
        headerLayout->addWidget(lblNote);
        mainLayout->addLayout(headerLayout);

        // --- ÁREA CENTRAL: OSCILOSCÓPIO GRÁFICO (TRIGGER ZERO-CROSSING) ---
        m_scope = new WaveformDisplay(this);
        g_display = m_scope;
        mainLayout->addWidget(m_scope, 1);

        // --- PAINEL INTERMEDIÁRIO DE TELEMETRIA ACÚSTICA ---
        QHBoxLayout* panelLayout = new QHBoxLayout();

        QGroupBox* grpFreq = new QGroupBox("TELEMETRIA ACÚSTICA (TEMPO REAL)", this);
        QVBoxLayout* vFreq = new QVBoxLayout(grpFreq);
        lblFreqValue = new QLabel("0.00 Hz", this);
        lblFreqValue->setStyleSheet("font-size: 26px; font-weight: bold; color: #00ffaa; font-family: monospace;");
        lblFreqValue->setAlignment(Qt::AlignCenter);
        lblBaseFreq = new QLabel("Base: 0.00 Hz | Vel: 0", this);
        lblBaseFreq->setAlignment(Qt::AlignCenter);
        vFreq->addWidget(lblFreqValue);
        vFreq->addWidget(lblBaseFreq);
        panelLayout->addWidget(grpFreq, 1);

        QGroupBox* grpComp = new QGroupBox("COMPARAÇÃO ORIGINAL vs MODULADO (4.3)", this);
        QVBoxLayout* vComp = new QVBoxLayout(grpComp);
        lblRefFreqText = new QLabel("Ref. Original (f₀): Nenhuma fixada", this);
        lblRefFreqText->setStyleSheet("color: #38bdf8; font-weight: bold;");
        lblDeltaFreq = new QLabel("Desvio Delta (Δf): 0.00 Hz (0.0%)", this);
        lblDeltaPeriod = new QLabel("Variação Período (ΔT): 0.00 ms (Estável)", this);
        lblDeltaPeriod->setStyleSheet("color: #e0f2f1;");
        vComp->addWidget(lblRefFreqText);
        vComp->addWidget(lblDeltaFreq);
        vComp->addWidget(lblDeltaPeriod);
        panelLayout->addWidget(grpComp, 1);

        QGroupBox* grpMod = new QGroupBox("TELEMETRIA UMP (RESOLUÇÃO)", this);
        QVBoxLayout* vMod = new QVBoxLayout(grpMod);

        QHBoxLayout* hPb = new QHBoxLayout();
        lblPitch = new QLabel("Pitch: 0.00 st", this);
        lblPitchHex = new QLabel("[0x80000000]", this);
        lblPitchHex->setStyleSheet("color: #00e5ff; font-weight: bold;");
        hPb->addWidget(lblPitch);
        hPb->addStretch();
        hPb->addWidget(lblPitchHex);

        barPitch = new QProgressBar(this);
        barPitch->setRange(-100, 100);
        barPitch->setValue(0);
        barPitch->setTextVisible(false);
        barPitch->setFixedHeight(8);

        QHBoxLayout* hMod = new QHBoxLayout();
        lblMod = new QLabel("Mod Wheel (CC1): 0.0%", this);
        lblLfo = new QLabel("FM β: 0.00 | Cutoff: 100%", this);
        lblLfo->setStyleSheet("color: #80cbc4;");
        hMod->addWidget(lblMod);
        hMod->addStretch();
        hMod->addWidget(lblLfo);

        vMod->addLayout(hPb);
        vMod->addWidget(barPitch);
        vMod->addLayout(hMod);
        panelLayout->addWidget(grpMod, 1);

        mainLayout->addLayout(panelLayout);

        // --- PAINEL INFERIOR: COCKPIT DE DEMONSTRAÇÃO MIDI 2.0 vs ZIPPER NOISE ---
        QGroupBox* grpZipper = new QGroupBox("EXPERIMENTO ACADÊMICO: RESOLUÇÃO MIDI 2.0 (32-BIT) vs RUÍDO DE ZÍPER MIDI 1.0 (7-BIT)", this);
        QVBoxLayout* vZipper = new QVBoxLayout(grpZipper);
        vZipper->setSpacing(8);

        // Linha 1: Seletor de Modo e Badge de Resolução
        QHBoxLayout* hModeSelect = new QHBoxLayout();
        rbMidi2 = new QRadioButton("🟢 Modo MIDI 2.0 UMP (32-bit Contínuo)", this);
        rbMidi1 = new QRadioButton("🔴 Modo MIDI 1.0 (7-bit / 14-bit com Degraus)", this);
        rbMidi2->setChecked(true);

        QButtonGroup* grpRadio = new QButtonGroup(this);
        grpRadio->addButton(rbMidi2);
        grpRadio->addButton(rbMidi1);

        connect(rbMidi2, &QRadioButton::toggled, this, &OscilloscopeWindow::onModeChanged);

        lblModeBadge = new QLabel("STATUS: MIDI 2.0 ATIVO (4.294.967.296 Passos / Sinal Contínuo sem Degraus)", this);
        lblModeBadge->setStyleSheet("background-color: #064e3b; color: #6ee7b7; border: 1px solid #10b981; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px;");

        hModeSelect->addWidget(rbMidi2);
        hModeSelect->addSpacing(16);
        hModeSelect->addWidget(rbMidi1);
        hModeSelect->addStretch();
        hModeSelect->addWidget(lblModeBadge);
        vZipper->addLayout(hModeSelect);

        // Linha 2: Slider de Modulação CC1 (32-bit nativo)
        QHBoxLayout* hRowMod = new QHBoxLayout();
        lblModCtrl = new QLabel("CC1 Modulação:   0.0% | UMP 32-bit: 0x00000000 | MIDI 1.0:   0/127 (7-bit)", this);
        lblModCtrl->setMinimumWidth(440);
        sliderMidiMod = new QSlider(Qt::Horizontal, this);
        sliderMidiMod->setRange(0, 10000);
        sliderMidiMod->setValue(0);
        sliderMidiMod->setStyleSheet(
            "QSlider::groove:horizontal { height: 6px; background: #050b0e; border: 1px solid #004d40; border-radius: 3px; }"
            "QSlider::sub-page:horizontal { background: #00e5ff; border-radius: 3px; }"
            "QSlider::handle:horizontal { background: #00ffaa; border: 1px solid #004d40; width: 14px; margin-top: -4px; margin-bottom: -4px; border-radius: 7px; }"
        );
        connect(sliderMidiMod, &QSlider::valueChanged, this, &OscilloscopeWindow::onModSliderChanged);
        hRowMod->addWidget(lblModCtrl);
        hRowMod->addWidget(sliderMidiMod, 1);
        vZipper->addLayout(hRowMod);

        // Linha 3: Slider de Filtro Cutoff CC74 (demonstração clássica de Zipper Noise)
        QHBoxLayout* hRowCutoff = new QHBoxLayout();
        lblCutoffCtrl = new QLabel("CC74 Cutoff: 18000 Hz | UMP 32-bit: 0xFFFFFFFF | MIDI 1.0: 127/127 (7-bit)", this);
        lblCutoffCtrl->setMinimumWidth(440);
        sliderMidiCutoff = new QSlider(Qt::Horizontal, this);
        sliderMidiCutoff->setRange(0, 10000);
        sliderMidiCutoff->setValue(10000);
        sliderMidiCutoff->setStyleSheet(
            "QSlider::groove:horizontal { height: 6px; background: #050b0e; border: 1px solid #004d40; border-radius: 3px; }"
            "QSlider::sub-page:horizontal { background: #38bdf8; border-radius: 3px; }"
            "QSlider::handle:horizontal { background: #38bdf8; border: 1px solid #004d40; width: 14px; margin-top: -4px; margin-bottom: -4px; border-radius: 7px; }"
        );
        connect(sliderMidiCutoff, &QSlider::valueChanged, this, &OscilloscopeWindow::onCutoffSliderChanged);
        hRowCutoff->addWidget(lblCutoffCtrl);
        hRowCutoff->addWidget(sliderMidiCutoff, 1);
        vZipper->addLayout(hRowCutoff);

        // Linha 4: Slider de Pitch Bend (32-bit nativo)
        QHBoxLayout* hRowPitch = new QHBoxLayout();
        lblPitchCtrl = new QLabel("Pitch Bend:  +0.00 st | UMP 32-bit: 0x80000000 | MIDI 1.0: 8192/16383 (14-bit)", this);
        lblPitchCtrl->setMinimumWidth(440);
        sliderMidiPitch = new QSlider(Qt::Horizontal, this);
        sliderMidiPitch->setRange(-10000, 10000);
        sliderMidiPitch->setValue(0);
        sliderMidiPitch->setStyleSheet(
            "QSlider::groove:horizontal { height: 6px; background: #050b0e; border: 1px solid #004d40; border-radius: 3px; }"
            "QSlider::sub-page:horizontal { background: #a78bfa; border-radius: 3px; }"
            "QSlider::handle:horizontal { background: #c084fc; border: 1px solid #004d40; width: 14px; margin-top: -4px; margin-bottom: -4px; border-radius: 7px; }"
        );
        connect(sliderMidiPitch, &QSlider::valueChanged, this, &OscilloscopeWindow::onPitchSliderChanged);
        hRowPitch->addWidget(lblPitchCtrl);
        hRowPitch->addWidget(sliderMidiPitch, 1);
        vZipper->addLayout(hRowPitch);

        // Linha 5: Botões de Demonstração e Teste
        QHBoxLayout* hDemoButtons = new QHBoxLayout();
        btnTestNote = new QPushButton("▶ Tocar Nota de Teste (C4 / 261.63 Hz)", this);
        btnTestNote->setToolTip("Inicia a emissão sonora direta sem depender de teclado externo ou VMPK (Tecla T)");
        connect(btnTestNote, &QPushButton::clicked, this, &OscilloscopeWindow::onToggleTestNote);

        btnAutoSweep = new QPushButton("🔄 Iniciar Varredura A/B (Zipper Demo)", this);
        btnAutoSweep->setToolTip("Executa varredura automática suave de Cutoff e CC1 para alternar em tempo real entre MIDI 1.0 e 2.0 (Tecla S)");
        connect(btnAutoSweep, &QPushButton::clicked, this, &OscilloscopeWindow::onToggleSweep);

        btnResetParams = new QPushButton("↺ Resetar Controles", this);
        connect(btnResetParams, &QPushButton::clicked, this, &OscilloscopeWindow::onResetParameters);

        QLabel* lblShortcuts = new QLabel("Atalhos: [T] Nota Teste | [S] Varredura A/B | [1] MIDI 1.0 | [2] MIDI 2.0 | [Espaço] Fixar Ref.", this);
        lblShortcuts->setStyleSheet("color: #80cbc4; font-size: 11px;");

        hDemoButtons->addWidget(btnTestNote);
        hDemoButtons->addWidget(btnAutoSweep);
        hDemoButtons->addWidget(btnResetParams);
        hDemoButtons->addStretch();
        hDemoButtons->addWidget(lblShortcuts);
        vZipper->addLayout(hDemoButtons);

        mainLayout->addWidget(grpZipper);

        setCentralWidget(central);

        // Timer de Telemetria e Renderização (60 FPS)
        QTimer* timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &OscilloscopeWindow::updateTelemetry);
        timer->start(16);

        // Timer de Varredura Automática A/B
        m_sweepTimer = new QTimer(this);
        connect(m_sweepTimer, &QTimer::timeout, this, &OscilloscopeWindow::onSweepTick);
    }

    ~OscilloscopeWindow() override {
        g_display = nullptr;
    }

protected:
    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_Space) {
            onCaptureReference();
        } else if (event->key() == Qt::Key_C) {
            onClearReference();
        } else if (event->key() == Qt::Key_T) {
            onToggleTestNote();
        } else if (event->key() == Qt::Key_S) {
            onToggleSweep();
        } else if (event->key() == Qt::Key_1) {
            rbMidi1->setChecked(true);
        } else if (event->key() == Qt::Key_2) {
            rbMidi2->setChecked(true);
        } else if (event->key() == Qt::Key_Up) {
            int cur = sliderMidiMod->value();
            sliderMidiMod->setValue((cur + 500 > 10000) ? 10000 : cur + 500);
        } else if (event->key() == Qt::Key_Down) {
            int cur = sliderMidiMod->value();
            sliderMidiMod->setValue((cur - 500 < 0) ? 0 : cur - 500);
        } else {
            QMainWindow::keyPressEvent(event);
        }
    }

private slots:
    void onCaptureReference() {
        if (g_synth.note_active.load()) {
            m_scope->captureReference();
            m_refFreq = g_synth.target_freq.load();
            double refT = (m_refFreq > 0.0) ? (1000.0 / m_refFreq) : 0.0;
            lblRefFreqText->setText(QString("Ref. Original (f₀): %1 Hz (T₀ = %2 ms)")
                                      .arg(m_refFreq, 0, 'f', 2).arg(refT, 0, 'f', 3));
            lblRefFreqText->setStyleSheet("color: #38bdf8; font-weight: bold;");
        }
    }

    void onClearReference() {
        m_scope->clearReference();
        m_refFreq = 0.0;
        lblRefFreqText->setText("Ref. Original (f₀): Nenhuma fixada");
        lblRefFreqText->setStyleSheet("color: #80cbc4;");
        lblDeltaFreq->setText("Desvio Delta (Δf): 0.00 Hz (0.0%)");
        lblDeltaPeriod->setText("Variação Período (ΔT): 0.00 ms (Estável)");
    }

    void onModeChanged() {
        bool isMidi2 = rbMidi2->isChecked();
        g_synth.midi2_mode.store(isMidi2);
        if (isMidi2) {
            lblModeBadge->setText("STATUS: MIDI 2.0 ATIVO (4.294.967.296 Passos / Sinal Contínuo sem Degraus)");
            lblModeBadge->setStyleSheet("background-color: #064e3b; color: #6ee7b7; border: 1px solid #10b981; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px;");
        } else {
            lblModeBadge->setText("STATUS: MIDI 1.0 ATIVO (128 Passos CC / 16384 Passos PB - Zipper Noise Audível)");
            lblModeBadge->setStyleSheet("background-color: #7f1d1d; color: #fca5a5; border: 1px solid #ef4444; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px;");
        }
        onModSliderChanged(sliderMidiMod->value());
        onCutoffSliderChanged(sliderMidiCutoff->value());
        onPitchSliderChanged(sliderMidiPitch->value());
    }

    void onModSliderChanged(int raw) {
        double norm = raw / 10000.0;
        bool isMidi2 = g_synth.midi2_mode.load();
        if (!isMidi2) {
            int step7 = (int)std::round(norm * 127.0);
            norm = step7 / 127.0;
        }
        g_synth.mod_depth.store(norm);
        updateControlLabels();
    }

    void onCutoffSliderChanged(int raw) {
        double norm = raw / 10000.0;
        bool isMidi2 = g_synth.midi2_mode.load();
        if (!isMidi2) {
            int step7 = (int)std::round(norm * 127.0);
            norm = step7 / 127.0;
        }
        g_synth.filter_cutoff_norm.store(norm);
        updateControlLabels();
    }

    void onPitchSliderChanged(int raw) {
        double norm = raw / 10000.0; // -1.0 a +1.0
        bool isMidi2 = g_synth.midi2_mode.load();
        if (!isMidi2) {
            // MIDI 1.0 Pitch Bend: padrão de 14 bits (16384 degraus, centro 8192)
            int step14 = (int)std::round(((norm + 1.0) * 0.5) * 16383.0);
            norm = (step14 / 16383.0) * 2.0 - 1.0;
        }
        g_synth.pitch_offset_semitones.store(norm * 2.0);
        uint32_t p32 = (uint32_t)((norm + 1.0) * 0.5 * 4294967295.0);
        g_synth.last_pitch32.store(p32);
        updateControlLabels();
    }

    void updateControlLabels() {
        double mod_d = g_synth.mod_depth.load();
        double cut_n = g_synth.filter_cutoff_norm.load();
        double pitch_st = g_synth.pitch_offset_semitones.load();

        uint32_t mod32 = (uint32_t)(mod_d * 4294967295.0);
        uint8_t mod7 = (uint8_t)std::round(mod_d * 127.0);

        uint32_t cut32 = (uint32_t)(cut_n * 4294967295.0);
        uint8_t cut7 = (uint8_t)std::round(cut_n * 127.0);
        double fc = 80.0 * std::pow(18000.0 / 80.0, cut_n);

        uint32_t p32 = g_synth.last_pitch32.load();
        int pitch14 = (int)std::round(((pitch_st / 2.0) + 1.0) * 0.5 * 16383.0);
        if (pitch14 < 0) pitch14 = 0; else if (pitch14 > 16383) pitch14 = 16383;

        lblModCtrl->setText(QString("CC1 Modulação:   %1% | UMP 32-bit: 0x%2 | MIDI 1.0:   %3/127 (7-bit)")
            .arg(mod_d * 100.0, 5, 'f', 1)
            .arg(mod32, 8, 16, QChar('0')).toUpper()
            .arg(mod7, 3));

        lblCutoffCtrl->setText(QString("CC74 Cutoff: %1 Hz | UMP 32-bit: 0x%2 | MIDI 1.0: %3/127 (7-bit)")
            .arg(fc, 5, 'f', 0)
            .arg(cut32, 8, 16, QChar('0')).toUpper()
            .arg(cut7, 3));

        lblPitchCtrl->setText(QString("Pitch Bend:  %1%2 st | UMP 32-bit: 0x%3 | MIDI 1.0: %4/16383 (14-bit)")
            .arg(pitch_st >= 0 ? "+" : "").arg(pitch_st, 4, 'f', 2)
            .arg(p32, 8, 16, QChar('0')).toUpper()
            .arg(pitch14, 5));
    }

    void onToggleTestNote() {
        bool active = g_synth.note_active.load();
        if (!active) {
            g_synth.current_note.store(60);
            g_synth.target_freq.store(261.63);
            g_synth.last_vel16.store(0xC000);
            g_synth.target_amp.store(0.35);
            g_synth.note_active.store(true);
            btnTestNote->setText("⏹ Parar Nota de Teste (C4)");
            btnTestNote->setStyleSheet("background-color: #b91c1c; color: white; font-weight: bold; padding: 6px 14px; border-radius: 4px;");
        } else {
            g_synth.target_amp.store(0.0);
            g_synth.note_active.store(false);
            btnTestNote->setText("▶ Tocar Nota de Teste (C4 / 261.63 Hz)");
            btnTestNote->setStyleSheet("background-color: #004d40; color: #e0f2f1; font-weight: bold; padding: 6px 14px; border-radius: 4px; border: 1px solid #00bfa5;");
        }
    }

    void onToggleSweep() {
        m_sweeping = !m_sweeping;
        if (m_sweeping) {
            btnAutoSweep->setText("⏹ Parar Varredura A/B");
            btnAutoSweep->setStyleSheet("background-color: #b91c1c; color: white; font-weight: bold; padding: 6px 14px; border-radius: 4px;");
            m_sweepPhase = 0.0;
            m_sweepTimer->start(20);
        } else {
            btnAutoSweep->setText("🔄 Iniciar Varredura A/B (Zipper Demo)");
            btnAutoSweep->setStyleSheet("background-color: #004d40; color: #e0f2f1; font-weight: bold; padding: 6px 14px; border-radius: 4px; border: 1px solid #00bfa5;");
            m_sweepTimer->stop();
        }
    }

    void onSweepTick() {
        if (!m_sweeping) return;
        m_sweepPhase += 0.035;
        if (m_sweepPhase >= 2.0 * M_PI) m_sweepPhase -= 2.0 * M_PI;

        double wave = std::sin(m_sweepPhase) * 0.5 + 0.5;

        // Varia Cutoff entre 5% e 95%
        int cutoffVal = (int)((0.05 + wave * 0.90) * 10000.0);
        // Varia Modulação entre 0% e 75%
        int modVal = (int)((wave * 0.75) * 10000.0);

        sliderMidiCutoff->blockSignals(true);
        sliderMidiCutoff->setValue(cutoffVal);
        sliderMidiCutoff->blockSignals(false);
        onCutoffSliderChanged(cutoffVal);

        sliderMidiMod->blockSignals(true);
        sliderMidiMod->setValue(modVal);
        sliderMidiMod->blockSignals(false);
        onModSliderChanged(modVal);
    }

    void onResetParameters() {
        sliderMidiMod->setValue(0);
        sliderMidiCutoff->setValue(10000);
        sliderMidiPitch->setValue(0);
        onModSliderChanged(0);
        onCutoffSliderChanged(10000);
        onPitchSliderChanged(0);
    }

    void updateTelemetry() {
        m_scope->update();

        // Auto-reconexão inteligente se a porta não estava aberta no início
        if (!m_portOpened && m_midi) {
            try {
                if (m_midi->getPortCount() > 0) {
                    m_midi->openPort(0);
                    m_portOpened = true;
                    m_portName = QString::fromStdString(m_midi->getPortName(0));
                    m_midi->setCallback(&midi_callback);
                    m_midi->ignoreTypes(false, false, false);
                    lblStatus->setText(QString("● CONECTADO: %1").arg(m_portName));
                    lblStatus->setStyleSheet("font-size: 13px; font-weight: bold; color: #00ffaa;");
                }
            } catch (...) {}
        }

        bool active = g_synth.note_active.load();
        double base_f = g_synth.target_freq.load();
        double pitch_st = g_synth.pitch_offset_semitones.load();
        double mod_d = g_synth.mod_depth.load();
        double cut_n = g_synth.filter_cutoff_norm.load();
        uint16_t vel16 = g_synth.last_vel16.load();
        uint32_t pitch32 = g_synth.last_pitch32.load();
        uint8_t note = g_synth.current_note.load();

        double eff_f = 0.0;
        if (active) {
            eff_f = g_synth.instant_freq.load();
            lblFreqValue->setText(QString("%1 Hz").arg(eff_f, 6, 'f', 2));
            lblBaseFreq->setText(QString("Nota: %1 | Base: %2 Hz | Vel (16-bit): %3")
                                    .arg(note).arg(base_f, 0, 'f', 2).arg(vel16));
            lblNote->setText(QString("NOTA: %1 (ATIVA)").arg(note));
            lblNote->setStyleSheet("font-size: 13px; font-weight: bold; color: #00ffaa;");
        } else {
            lblFreqValue->setText("---.-- Hz");
            lblNote->setText("NOTA: NENHUMA");
            lblNote->setStyleSheet("font-size: 13px; font-weight: bold; color: #80cbc4;");
        }

        if (m_refFreq > 0.001 && active) {
            double delta_f = eff_f - m_refFreq;
            double pct = (delta_f / m_refFreq) * 100.0;
            double cur_T = (eff_f > 0.0) ? (1000.0 / eff_f) : 0.0;
            double ref_T = (1000.0 / m_refFreq);
            double delta_T = cur_T - ref_T;

            QString compStr = (delta_f >= 0) ? "+" : "";
            lblDeltaFreq->setText(QString("Desvio Delta (Δf): %1%2 Hz (%3%4%)")
                                    .arg(compStr).arg(delta_f, 0, 'f', 2)
                                    .arg(compStr).arg(pct, 0, 'f', 2));

            QString stateT = (delta_T < -0.01) ? "(Comprimido)" : (delta_T > 0.01 ? "(Alargado)" : "(Estável)");
            lblDeltaPeriod->setText(QString("Variação Período (ΔT): %1%2 ms %3")
                                      .arg(compStr).arg(delta_T, 0, 'f', 3).arg(stateT));

            if (std::abs(delta_f) > 0.5) {
                lblDeltaFreq->setStyleSheet("color: #00ffaa; font-weight: bold;");
            } else {
                lblDeltaFreq->setStyleSheet("color: #e0f2f1;");
            }
        }

        lblPitch->setText(QString("Pitch: %1%2 st")
                            .arg(pitch_st >= 0 ? "+" : "").arg(pitch_st, 0, 'f', 2));
        lblPitchHex->setText(QString("[0x%1]").arg(pitch32, 8, 16, QChar('0')).toUpper());
        barPitch->setValue((int)((pitch_st / 2.0) * 100));

        double beta = mod_d * 1.8;
        lblMod->setText(QString("Mod Wheel (CC1): %1%").arg(mod_d * 100.0, 0, 'f', 1));
        lblLfo->setText(QString("FM β: %1 | Cutoff: %2%").arg(beta, 0, 'f', 2).arg(cut_n * 100.0, 0, 'f', 0));
    }

private:
    RtMidiIn* m_midi;
    bool m_portOpened;
    QString m_portName;
    WaveformDisplay* m_scope;
    double m_refFreq;

    bool m_sweeping;
    double m_sweepPhase;
    QTimer* m_sweepTimer;

    QLabel* lblStatus;
    QLabel* lblNote;
    QLabel* lblFreqValue;
    QLabel* lblBaseFreq;
    QLabel* lblPitch;
    QLabel* lblPitchHex;
    QLabel* lblMod;
    QLabel* lblLfo;
    QLabel* lblRefFreqText;
    QLabel* lblDeltaFreq;
    QLabel* lblDeltaPeriod;
    QProgressBar* barPitch;

    QPushButton* btnFreezeRef;
    QPushButton* btnClearRef;

    QRadioButton* rbMidi2;
    QRadioButton* rbMidi1;
    QLabel* lblModeBadge;

    QLabel* lblModCtrl;
    QSlider* sliderMidiMod;

    QLabel* lblCutoffCtrl;
    QSlider* sliderMidiCutoff;

    QLabel* lblPitchCtrl;
    QSlider* sliderMidiPitch;

    QPushButton* btnTestNote;
    QPushButton* btnAutoSweep;
    QPushButton* btnResetParams;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate        = 48000;
    config.dataCallback      = audio_callback;

    ma_device device;
    bool audio_ok = (ma_device_init(nullptr, &config, &device) == MA_SUCCESS);
    if (audio_ok) {
        ma_device_start(&device);
    } else {
        std::cerr << "[AVISO] Falha ao iniciar dispositivo de áudio playback.\n";
    }

    RtMidiIn* midiIn = nullptr;
    try {
        midiIn = new RtMidiIn();
    } catch (const std::exception& e) {
        std::cerr << "[AVISO] Falha ao criar RtMidiIn: " << e.what() << "\n";
    }

    bool portOpened = false;
    QString portName = "Nenhuma porta";

    if (midiIn) {
        try {
            midiIn->openVirtualPort("UMP Audio Sink Input");
            portOpened = true;
            portName = "UMP Audio Sink Input (Virtual)";
        } catch (...) {
            try {
                if (midiIn->getPortCount() > 0) {
                    midiIn->openPort(0);
                    portOpened = true;
                    portName = QString::fromStdString(midiIn->getPortName(0));
                }
            } catch (...) {}
        }

        if (portOpened) {
            try {
                midiIn->setCallback(&midi_callback);
                midiIn->ignoreTypes(false, false, false);
            } catch (...) {}
        }
    }

    OscilloscopeWindow win(midiIn, portOpened, portName);
    win.show();

    int ret = app.exec();

    if (midiIn) {
        try {
            midiIn->cancelCallback();
            midiIn->closePort();
        } catch (...) {}
        delete midiIn;
    }

    if (audio_ok) {
        ma_device_stop(&device);
        ma_device_uninit(&device);
    }

    return ret;
}
