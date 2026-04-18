#include "gui.h"
#include "converter.h"
#include <QVBoxLayout>
#include <QHeaderView>


MidiWorker::MidiWorker(RtMidiIn* port, QObject *parent)
    : QThread(parent), m_port(port), m_last_note(60) {}

MidiWorker::~MidiWorker() {
    requestInterruption();
    wait();
}

void MidiWorker::run() {
    std::vector<unsigned char> message;
    while (!isInterruptionRequested()) {
        m_port->getMessage(&message);
        if (message.empty()) {
            QThread::msleep(1); // Evita 100% de uso da CPU
            continue;
        }

        if (message.size() >= 2) {
            uint8_t status = message[0] & 0xF0;
            uint8_t channel = message[0] & 0x0F;
            uint8_t data1 = message[1];
            uint8_t data2 = (message.size() > 2) ? message[2] : 0;

            UMPMessage ump_msg(0, 0);
            bool has_ump = false;
            QString original_str = "-";

            if (status == 0x90 && data2 > 0) { // Note On
                m_last_note = data1;
                original_str = QString("Vel: %1").arg(data2);
                uint32_t v2 = midi1_to_midi2_velocity(data2);
                ump_msg = create_midi2_note_on(data1, v2, channel);
                has_ump = true;
            } else if (status == 0x80 || (status == 0x90 && data2 == 0)) { // Note Off
                original_str = QString("Vel: %1").arg(data2);
                uint32_t v2 = midi1_to_midi2_velocity(data2);
                ump_msg = create_midi2_note_off(data1, v2, channel);
                has_ump = true;
            } else if (status == 0xE0) { // Pitch Bend
                uint16_t pitch = data1 | (data2 << 7);
                original_str = QString("Pitch: %1").arg(pitch);
                uint32_t p32 = midi1_to_midi2_pitch(pitch);
                ump_msg = create_midi2_pitch_bend(p32, channel);
                emit pitch_signal((int)((p32 / (double)0xFFFFFFFF) * 100));
                has_ump = true;
            } else if (status == 0xB0) { // Control Change
                original_str = QString("Val: %1").arg(data2);
                uint32_t v32 = midi1_to_midi2_32bit(data2);
                ump_msg = create_midi2_control_change(data1, v32, channel);
                has_ump = true;
            }

            if (has_ump) {
                QMap<QString, QString> data = ump_msg.analyze();
                QVariantMap vmap;
                for (auto key : data.keys()) vmap[key] = data[key];
                vmap["original"] = original_str;
                emit log_signal(vmap);
            }
        }
    }
}


MainWindow::MainWindow(RtMidiIn* port, QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("Analisador MIDI 2.0 UMP - TCC IFPB");
    setMinimumSize(750, 450);

    QWidget* container = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(container);

    table = new QTableWidget(0, 6, this);
    table->setHorizontalHeaderLabels({
        "Mensagem", "Ch", "Alvo", "Valor Original", "Valor Convertido", "Raw Words (UMP 64-bit)"
    });
    table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);

    bar = new QProgressBar(this);

    btn_simular = new QPushButton("Simular Pitch Bend (Teste de Software)", this);
    connect(btn_simular, &QPushButton::clicked, this, &MainWindow::simular_pitch_bend);

    QString portName = "Desconhecido";
    try { portName = QString::fromStdString(port->getPortName()); } catch (...) {}

    layout->addWidget(new QLabel(QString("Hardware Conectado: %1").arg(portName), this));
    layout->addWidget(btn_simular);
    layout->addWidget(new QLabel("Resolucao Pitch Bend (32-bit):", this));
    layout->addWidget(bar);
    layout->addWidget(new QLabel("Analisador de Pacotes UMP em Tempo Real:", this));
    layout->addWidget(table);
    setCentralWidget(container);

    worker = new MidiWorker(port, this);
    connect(worker, &MidiWorker::log_signal, this, &MainWindow::add_table_row);
    connect(worker, &MidiWorker::pitch_signal, bar, &QProgressBar::setValue);
    worker->start();
}

MainWindow::~MainWindow() {
    worker->requestInterruption();
    worker->wait();
}

void MainWindow::simular_pitch_bend() {
    uint16_t val_midi1 = 8192; // <- Mude de 0 para 8192 (O novo centro!)
    uint32_t p32 = midi1_to_midi2_pitch(val_midi1);
    UMPMessage ump_msg = create_midi2_pitch_bend(p32, 0);

    bar->setValue((int)((p32 / (double)0xFFFFFFFF) * 100));

    QMap<QString, QString> data = ump_msg.analyze();
    QVariantMap vmap;
    for (auto key : data.keys()) vmap[key] = data[key];
    vmap["original"] = QString("Pitch: %1 (Simulado)").arg(val_midi1);
    
    add_table_row(vmap);
}

void MainWindow::add_table_row(const QVariantMap& data) {
    int row_pos = table->rowCount();
    table->insertRow(row_pos);

    table->setItem(row_pos, 0, new QTableWidgetItem(data.value("type").toString()));
    table->setItem(row_pos, 1, new QTableWidgetItem(data.value("channel").toString()));
    table->setItem(row_pos, 2, new QTableWidgetItem(data.value("target").toString()));
    table->setItem(row_pos, 3, new QTableWidgetItem(data.value("original").toString()));
    table->setItem(row_pos, 4, new QTableWidgetItem(data.value("value").toString()));

    QString raw_words = QString("%1 | %2").arg(data.value("raw_w1").toString(), data.value("raw_w2").toString());
    table->setItem(row_pos, 5, new QTableWidgetItem(raw_words));
    table->scrollToBottom();
}