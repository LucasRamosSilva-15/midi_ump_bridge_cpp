#include "gui.h"
#include "converter.h"
#include <QHeaderView>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QVBoxLayout>

MidiWorker::MidiWorker(RtMidiIn *port, QObject *parent)
    : QThread(parent), m_port(port), m_last_note(60) {}

MidiWorker::~MidiWorker() {
  requestInterruption();
  wait();
}

void MidiWorker::run() {
  std::vector<unsigned char> message;
  while (!isInterruptionRequested()) {
    try {
      m_port->getMessage(&message);
    } catch (RtMidiError &error) {
      QVariantMap vmap;
      vmap["type"] = "Erro MIDI";
      vmap["channel"] = "-";
      vmap["target"] = "-";
      vmap["original"] = QString::fromStdString(error.getMessage());
      vmap["value"] = "-";
      vmap["raw_w1"] = "-";
      vmap["raw_w2"] = "-";
      emit log_signal(vmap);
      requestInterruption();
      break;
    }

    if (message.empty()) {
      QThread::msleep(2);
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

      if (status == 0x90 && data2 > 0) {
        m_last_note = data1;
        original_str = QString("Vel: %1").arg(data2);
        uint32_t v2 = midi1_to_midi2_velocity(data2);
        ump_msg = create_midi2_note_on(data1, static_cast<uint16_t>(v2), channel);
        has_ump = true;
      } else if (status == 0x80 || (status == 0x90 && data2 == 0)) {
        original_str = QString("Vel: %1").arg(data2);
        uint32_t v2 = midi1_to_midi2_velocity(data2);
        ump_msg = create_midi2_note_off(data1, static_cast<uint16_t>(v2), channel);
        has_ump = true;
      } else if (status == 0xE0) {
        uint16_t pitch = data1 | (data2 << 7);
        original_str = QString("Pitch: %1").arg(pitch);
        uint32_t p32 = midi1_to_midi2_pitch(pitch);
        ump_msg = create_midi2_pitch_bend(p32, channel);
        emit pitch_signal(static_cast<int>((pitch / 16383.0) * 100));
        has_ump = true;
      } else if (status == 0xB0) {
        original_str = QString("Val: %1").arg(data2);
        uint32_t v32 = midi1_to_midi2_32bit(data2);
        ump_msg = create_midi2_control_change(data1, v32, channel);
        has_ump = true;
      }

      if (has_ump) {
        QMap<QString, QString> data = ump_msg.analyze();
        QVariantMap vmap;
        for (auto key : data.keys())
          vmap[key] = data[key];
        vmap["original"] = original_str;
        emit log_signal(vmap);
      }
    }
  }
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), worker(nullptr) {
  setWindowTitle("Analisador MIDI 2.0 UMP - TCC IFPB");
  setMinimumSize(750, 450);

  QWidget *container = new QWidget(this);
  QVBoxLayout *layout = new QVBoxLayout(container);
  QHBoxLayout *port_layout = new QHBoxLayout();

  port_selector = new QComboBox(this);
  btn_refresh = new QPushButton("Atualizar", this);
  btn_connect = new QPushButton("Conectar", this);
  btn_disconnect = new QPushButton("Desconectar", this);
  status_label = new QLabel(this);

  port_layout->addWidget(port_selector, 1);
  port_layout->addWidget(btn_refresh);
  port_layout->addWidget(btn_connect);
  port_layout->addWidget(btn_disconnect);

  connect(btn_refresh, &QPushButton::clicked, this, &MainWindow::refresh_ports);
  connect(btn_connect, &QPushButton::clicked, this,
          &MainWindow::connect_selected_port);
  connect(btn_disconnect, &QPushButton::clicked, this,
          &MainWindow::disconnect_port);

  table = new QTableWidget(0, 6, this);
  table->setHorizontalHeaderLabels({"Mensagem", "Ch", "Alvo", "Valor Original",
                                    "Valor Convertido",
                                    "Raw Words (UMP 64-bit)"});
  table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);

  bar = new QProgressBar(this);

  btn_simular = new QPushButton("Simular Pitch Bend (Teste de Software)", this);
  connect(btn_simular, &QPushButton::clicked, this,
          &MainWindow::simular_pitch_bend);

  layout->addWidget(new QLabel("Entrada MIDI:", this));
  layout->addLayout(port_layout);
  layout->addWidget(status_label);
  layout->addWidget(btn_simular);
  layout->addWidget(new QLabel("Resolucao Pitch Bend (32-bit):", this));
  layout->addWidget(bar);
  layout->addWidget(
      new QLabel("Analisador de Pacotes UMP em Tempo Real:", this));
  layout->addWidget(table);
  setCentralWidget(container);

  refresh_ports();
}

MainWindow::~MainWindow() {
  stop_worker();
}

void MainWindow::refresh_ports() {
  port_selector->clear();

  try {
    RtMidiIn input;
    const unsigned int port_count = input.getPortCount();
    for (unsigned int i = 0; i < port_count; ++i) {
      port_selector->addItem(QString::fromStdString(input.getPortName(i)), i);
    }

    if (port_count == 0) {
      set_status("Nenhuma entrada MIDI encontrada.");
    } else {
      set_status(QString("%1 entrada(s) MIDI encontrada(s).").arg(port_count));
    }
  } catch (RtMidiError &error) {
    set_status(QString("Erro ao listar portas MIDI: %1")
                   .arg(QString::fromStdString(error.getMessage())));
  }

  btn_connect->setEnabled(port_selector->count() > 0);
  btn_disconnect->setEnabled(worker != nullptr);
}

void MainWindow::connect_selected_port() {
  if (port_selector->currentIndex() < 0) {
    set_status("Selecione uma entrada MIDI antes de conectar.");
    return;
  }

  const unsigned int port_index = port_selector->currentData().toUInt();
  const QString port_name = port_selector->currentText();

  stop_worker();

  try {
    auto new_port = std::make_unique<RtMidiIn>();
    new_port->ignoreTypes(false, false, false);
    new_port->openPort(port_index);
    midi_port = std::move(new_port);
  } catch (RtMidiError &error) {
    midi_port.reset();
    QMessageBox::warning(this, "Erro MIDI",
                         QString::fromStdString(error.getMessage()));
    set_status("Falha ao conectar a entrada MIDI.");
    return;
  }

  start_worker();
  set_status(QString("Conectado: %1").arg(port_name));
}

void MainWindow::disconnect_port() {
  stop_worker();
  midi_port.reset();
  set_status("Entrada MIDI desconectada.");
}

void MainWindow::start_worker() {
  if (!midi_port) {
    return;
  }

  worker = new MidiWorker(midi_port.get(), this);
  connect(worker, &MidiWorker::log_signal, this, &MainWindow::add_table_row);
  connect(worker, &MidiWorker::pitch_signal, bar, &QProgressBar::setValue);
  worker->start();

  btn_connect->setEnabled(false);
  btn_disconnect->setEnabled(true);
}

void MainWindow::stop_worker() {
  if (!worker) {
    return;
  }

  worker->requestInterruption();
  worker->wait();
  delete worker;
  worker = nullptr;

  btn_connect->setEnabled(port_selector->count() > 0);
  btn_disconnect->setEnabled(false);
}

void MainWindow::set_status(const QString &text) {
  status_label->setText(QString("Status: %1").arg(text));
}

void MainWindow::simular_pitch_bend() {
  uint16_t val_midi1 = 8192;
  uint32_t p32 = midi1_to_midi2_pitch(val_midi1);
  UMPMessage ump_msg = create_midi2_pitch_bend(p32, 0);

  bar->setValue(static_cast<int>((val_midi1 / 16383.0) * 100));

  QMap<QString, QString> data = ump_msg.analyze();
  QVariantMap vmap;
  for (auto key : data.keys())
    vmap[key] = data[key];
  vmap["original"] = QString("Pitch: %1 (Simulado)").arg(val_midi1);

  add_table_row(vmap);
}

void MainWindow::add_table_row(const QVariantMap &data) {
  constexpr int kMaxRows = 1000;
  while (table->rowCount() >= kMaxRows) {
    table->removeRow(0);
  }

  int row_pos = table->rowCount();
  table->insertRow(row_pos);

  table->setItem(row_pos, 0,
                 new QTableWidgetItem(data.value("type").toString()));
  table->setItem(row_pos, 1,
                 new QTableWidgetItem(data.value("channel").toString()));
  table->setItem(row_pos, 2,
                 new QTableWidgetItem(data.value("target").toString()));
  table->setItem(row_pos, 3,
                 new QTableWidgetItem(data.value("original").toString()));
  table->setItem(row_pos, 4,
                 new QTableWidgetItem(data.value("value").toString()));

  QString raw_words = QString("%1 | %2").arg(data.value("raw_w1").toString(),
                                             data.value("raw_w2").toString());
  table->setItem(row_pos, 5, new QTableWidgetItem(raw_words));
  table->scrollToBottom();
}