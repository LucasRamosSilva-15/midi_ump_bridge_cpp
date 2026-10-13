
#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QProgressBar>
#include <QStatusBar>
#include <QTimer>
#include <QKeyEvent>

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
    std::atomic<uint32_t> last_pitch32{0x80000000};
    std::atomic<uint16_t> last_vel16{0};
    std::atomic<double> instant_freq{261.63};

    double phase{0.0};
    double lfo_phase{0.0};
};

static SynthState g_synth;
static WaveformDisplay* g_display = nullptr;

void audio_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    (void)pInput;
    float* pOut = (float*)pOutput;
    double sampleRate = (double)pDevice->sampleRate;
    const double two_pi = 2.0 * M_PI;
    const double lfo_freq = 5.5;
    const double lfo_inc = two_pi * lfo_freq / sampleRate;

    std::vector<float> monoSamples(frameCount);

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
        double mod_st = lfo_val * (depth * 3.5);

        double total_st = pitch_st + mod_st;
        double effective_freq = base_f * std::pow(2.0, total_st / 12.0);
        g_synth.instant_freq.store(effective_freq);

        double phase_inc = two_pi * effective_freq / sampleRate;
        g_synth.phase += phase_inc;
        if (g_synth.phase >= two_pi) g_synth.phase -= two_pi;

        double tremolo = 1.0 - (depth * 0.3 * (1.0 + lfo_val) * 0.5);
        float sample = (float)(std::sin(g_synth.phase) * cur_amp * tremolo);
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
            }
        }
    }
}

class OscilloscopeWindow : public QMainWindow {
public:
    OscilloscopeWindow(RtMidiIn* midi, QWidget *parent = nullptr)
        : QMainWindow(parent), m_midi(midi), m_refFreq(0.0)
    {
        setWindowTitle("UMP Audio Oscilloscope - Comparativo Som Original vs Modulado (TCC 4.3)");
        resize(920, 680);
        setStyleSheet(
            "QMainWindow { background-color: #10151a; color: #e0f2f1; }"
            "QLabel { color: #b2dfdb; font-family: monospace; }"
            "QGroupBox { font-weight: bold; border: 1px solid #004d40; border-radius: 6px; margin-top: 12px; padding: 12px; background-color: #080d10; color: #80cbc4; }"
            "QGroupBox::title { subcontrol-origin: margin; left: 14px; padding: 0 4px; }"
            "QProgressBar { border: 1px solid #004d40; border-radius: 4px; text-align: center; background-color: #050b0e; color: #e0f2f1; }"
            "QProgressBar::chunk { background-color: #00e5ff; }"
            "QPushButton { background-color: #004d40; color: #e0f2f1; border: 1px solid #00bfa5; border-radius: 4px; padding: 6px 14px; font-weight: bold; font-family: sans-serif; }"
            "QPushButton:hover { background-color: #00695c; }"
            "QPushButton:pressed { background-color: #00897b; }"
        );

        QWidget* central = new QWidget(this);
        QVBoxLayout* mainLayout = new QVBoxLayout(central);
        mainLayout->setContentsMargins(16, 16, 16, 16);
        mainLayout->setSpacing(12);

        QHBoxLayout* headerLayout = new QHBoxLayout();
        lblStatus = new QLabel("● CONECTADO: UMP Audio Sink Input", this);
        lblStatus->setStyleSheet("font-size: 13px; font-weight: bold; color: #00ffaa;");

        btnFreezeRef = new QPushButton("📌 Fixar Som Original (Referência)", this);
        btnFreezeRef->setToolTip("Congela a onda do som original como linha azul para comparacao direta com o som modulado");
        connect(btnFreezeRef, &QPushButton::clicked, this, &OscilloscopeWindow::onCaptureReference);

        btnClearRef = new QPushButton("✕ Limpar Referência", this);
        connect(btnClearRef, &QPushButton::clicked, this, &OscilloscopeWindow::onClearReference);

        lblNote = new QLabel("NOTA: NENHUMA", this);
        lblNote->setStyleSheet("font-size: 13px; font-weight: bold; color: #80cbc4;");

        headerLayout->addWidget(lblStatus);
        headerLayout->addSpacing(16);
        headerLayout->addWidget(btnFreezeRef);
        headerLayout->addWidget(btnClearRef);
        headerLayout->addStretch();
        headerLayout->addWidget(lblNote);
        mainLayout->addLayout(headerLayout);

        m_scope = new WaveformDisplay(this);
        g_display = m_scope;
        mainLayout->addWidget(m_scope, 1);

        QHBoxLayout* panelLayout = new QHBoxLayout();

        QGroupBox* grpFreq = new QGroupBox("TELEMETRIA ACÚSTICA (TEMPO REAL)", this);
        QVBoxLayout* vFreq = new QVBoxLayout(grpFreq);
        lblFreqValue = new QLabel("0.00 Hz", this);
        lblFreqValue->setStyleSheet("font-size: 30px; font-weight: bold; color: #00ffaa; font-family: monospace;");
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

        QGroupBox* grpMod = new QGroupBox("CONTROLE UMP (ALTA RESOLUÇÃO)", this);
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
        barPitch->setFixedHeight(10);

        QHBoxLayout* hMod = new QHBoxLayout();
        lblMod = new QLabel("Mod Wheel (CC1): 0.0%", this);
        lblLfo = new QLabel("LFO: 5.5 Hz", this);
        lblLfo->setStyleSheet("color: #80cbc4;");
        hMod->addWidget(lblMod);
        hMod->addStretch();
        hMod->addWidget(lblLfo);

        barMod = new QProgressBar(this);
        barMod->setRange(0, 100);
        barMod->setValue(0);
        barMod->setFixedHeight(10);

        vMod->addLayout(hPb);
        vMod->addWidget(barPitch);
        vMod->addLayout(hMod);
        vMod->addWidget(barMod);
        panelLayout->addWidget(grpMod, 1);

        mainLayout->addLayout(panelLayout);
        setCentralWidget(central);

        QTimer* timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &OscilloscopeWindow::updateTelemetry);
        timer->start(16);
    }

protected:
    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_Space) {
            onCaptureReference();
        } else {
            QMainWindow::keyPressEvent(event);
        }
    }

private slots:
    void onCaptureReference() {
        if (g_synth.note_active.load()) {
            m_scope->captureReference();
            m_refFreq = g_synth.target_freq.load();
            double refT = (m_refFreq > 0) ? (1000.0 / m_refFreq) : 0.0;
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

    void updateTelemetry() {
        m_scope->update();

        bool active = g_synth.note_active.load();
        double base_f = g_synth.target_freq.load();
        double pitch_st = g_synth.pitch_offset_semitones.load();
        double mod_d = g_synth.mod_depth.load();
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

        if (m_refFreq > 0.0 && active) {
            double delta_f = eff_f - m_refFreq;
            double pct = (delta_f / m_refFreq) * 100.0;
            double cur_T = (eff_f > 0) ? (1000.0 / eff_f) : 0.0;
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

        double st_wobble = mod_d * 3.5;
        lblMod->setText(QString("Mod Wheel (CC1): %1% (±%2 st)").arg(mod_d * 100.0, 0, 'f', 1).arg(st_wobble, 0, 'f', 2));
        barMod->setValue((int)(mod_d * 100));
    }

private:
    RtMidiIn* m_midi;
    WaveformDisplay* m_scope;
    double m_refFreq;

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
    QProgressBar* barMod;
    QPushButton* btnFreezeRef;
    QPushButton* btnClearRef;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate        = 48000;
    config.dataCallback      = audio_callback;

    ma_device device;
    if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS) {
        std::cerr << "Falha ao iniciar audio.\n";
        return 1;
    }
    ma_device_start(&device);

    RtMidiIn* midiIn = new RtMidiIn();
    try {
        midiIn->openVirtualPort("UMP Audio Sink Input");
    } catch (...) {
        if (midiIn->getPortCount() > 0) midiIn->openPort(0);
    }
    midiIn->setCallback(&midi_callback);
    midiIn->ignoreTypes(false, false, false);

    OscilloscopeWindow win(midiIn);
    win.show();

    int ret = app.exec();

    delete midiIn;
    ma_device_stop(&device);
    ma_device_uninit(&device);
    return ret;
}
