#include "gui.h"
#include "converter.h"
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
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
        ump_msg =
            create_midi2_note_on(data1, static_cast<uint16_t>(v2), channel);
        has_ump = true;
      } else if (status == 0x80 || (status == 0x90 && data2 == 0)) {
        original_str = QString("Vel: %1").arg(data2);
        uint32_t v2 = midi1_to_midi2_velocity(data2);
        ump_msg =
            create_midi2_note_off(data1, static_cast<uint16_t>(v2), channel);
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
  resize(1200, 850);
  setMinimumSize(1100, 760);

  QWidget *container = new QWidget(this);
  container->setObjectName("mainContainer");
  QVBoxLayout *layout = new QVBoxLayout(container);
  layout->setContentsMargins(15, 10, 15, 15);
  layout->setSpacing(12);

  QLabel *title_label = new QLabel("MIDI2Bridge", this);
  title_label->setObjectName("titleLabel");
  title_label->setAlignment(Qt::AlignCenter);

  QLabel *subtitle_label =
      new QLabel("Conversor e Analisador MIDI 1.0 -> MIDI 2.0 UMP", this);
  subtitle_label->setObjectName("subtitleLabel");
  subtitle_label->setAlignment(Qt::AlignCenter);

  layout->addWidget(title_label);
  layout->addWidget(subtitle_label);

  QFrame *input_panel = new QFrame(this);
  input_panel->setObjectName("panel");
  input_panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
  QVBoxLayout *input_layout = new QVBoxLayout(input_panel);
  input_layout->setContentsMargins(15, 12, 15, 12);

  port_selector = new QComboBox(this);
  btn_refresh = new QPushButton("Atualizar", this);
  btn_refresh->setObjectName("secondaryButton");
  btn_connect = new QPushButton("Conectar", this);
  btn_connect->setObjectName("primaryButton");
  btn_disconnect = new QPushButton("Desconectar", this);
  btn_disconnect->setObjectName("dangerButton");
  status_label = new QLabel(this);
  status_label->setObjectName("statusLabel");

  QHBoxLayout *port_layout = new QHBoxLayout();
  port_layout->addWidget(port_selector, 1);
  port_layout->addWidget(btn_refresh);
  port_layout->addWidget(btn_connect);
  port_layout->addWidget(btn_disconnect);

  QLabel *lbl_entrada = new QLabel("Entrada MIDI:", this);
  lbl_entrada->setObjectName("sectionHeader");
  input_layout->addWidget(lbl_entrada);
  input_layout->addLayout(port_layout);
  input_layout->addWidget(status_label);

  layout->addWidget(input_panel);

  QFrame *pitch_panel = new QFrame(this);
  pitch_panel->setObjectName("panel");
  pitch_panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
  QVBoxLayout *pitch_layout = new QVBoxLayout(pitch_panel);
  pitch_layout->setContentsMargins(15, 12, 15, 12);

  bar = new QProgressBar(this);
  bar->setObjectName("pitchBar");
  btn_simular = new QPushButton("Simular Pitch Bend (Teste de Software)", this);
  btn_simular->setObjectName("secondaryButton");

  QLabel *lbl_pitch = new QLabel("Resolucao Pitch Bend (32-bit):", this);
  lbl_pitch->setObjectName("sectionHeader");
  pitch_layout->addWidget(lbl_pitch);
  pitch_layout->addWidget(bar);
  pitch_layout->addWidget(btn_simular);

  layout->addWidget(pitch_panel);

  QFrame *table_panel = new QFrame(this);
  table_panel->setObjectName("panel");
  table_panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  QVBoxLayout *table_layout = new QVBoxLayout(table_panel);
  table_layout->setContentsMargins(15, 12, 15, 12);

  table = new QTableWidget(0, 7, this);
  table->setHorizontalHeaderLabels({"#", "Mensagem", "Ch", "Alvo", "Valor Original",
                                    "Valor Convertido",
                                    "Raw Words (UMP 64-bit)"});
  table->verticalHeader()->setVisible(false);
  table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);
  table->horizontalHeader()->setStretchLastSection(true);
  table->setAlternatingRowColors(true);

  table->setColumnWidth(0, 45);
  table->setColumnWidth(1, 150);
  table->setColumnWidth(2, 60);
  table->setColumnWidth(3, 220);
  table->setColumnWidth(4, 200);
  table->setColumnWidth(5, 200);

  table->horizontalHeader()->setMinimumHeight(34);
  table->verticalHeader()->setDefaultSectionSize(32);
  table->setMinimumHeight(320);
  table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  QFont tableFont = table->font();
  tableFont.setPixelSize(13);
  table->setFont(tableFont);

  QHBoxLayout *table_header_layout = new QHBoxLayout();
  QLabel *lbl_table = new QLabel("Analisador de Pacotes UMP em Tempo Real:", this);
  lbl_table->setObjectName("sectionHeader");
  
  QLabel *badge = new QLabel("Monitor Ativo", this);
  badge->setObjectName("badgeLabel");
  badge->setAlignment(Qt::AlignCenter);

  table_header_layout->addWidget(lbl_table);
  table_header_layout->addStretch();
  table_header_layout->addWidget(badge);

  QFrame *h_line = new QFrame();
  h_line->setFrameShape(QFrame::HLine);
  h_line->setFrameShadow(QFrame::Sunken);
  h_line->setStyleSheet("border: 1px solid #c0c0c0; border-bottom: 1px solid #ffffff;");

  table_layout->addLayout(table_header_layout);
  table_layout->addWidget(h_line);
  table_layout->addWidget(table, 1);

  layout->addWidget(table_panel, 2);

  setCentralWidget(container);

  connect(btn_refresh, &QPushButton::clicked, this, &MainWindow::refresh_ports);
  connect(btn_connect, &QPushButton::clicked, this,
          &MainWindow::connect_selected_port);
  connect(btn_disconnect, &QPushButton::clicked, this,
          &MainWindow::disconnect_port);
  connect(btn_simular, &QPushButton::clicked, this,
          &MainWindow::simular_pitch_bend);

  refresh_ports();
  apply_skeuo_theme();
}

MainWindow::~MainWindow() { stop_worker(); }

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

  QTableWidgetItem *idx_item = new QTableWidgetItem(QString::number(row_pos + 1));
  idx_item->setTextAlignment(Qt::AlignCenter);
  table->setItem(row_pos, 0, idx_item);
  table->setItem(row_pos, 1,
                 new QTableWidgetItem(data.value("type").toString()));
  table->setItem(row_pos, 2,
                 new QTableWidgetItem(data.value("channel").toString()));
  table->setItem(row_pos, 3,
                 new QTableWidgetItem(data.value("target").toString()));
  table->setItem(row_pos, 4,
                 new QTableWidgetItem(data.value("original").toString()));
  table->setItem(row_pos, 5,
                 new QTableWidgetItem(data.value("value").toString()));

  QString raw_words = QString("%1 | %2").arg(data.value("raw_w1").toString(),
                                             data.value("raw_w2").toString());
  QTableWidgetItem *raw_item = new QTableWidgetItem(raw_words);
  QFont mono_font("Consolas");
  mono_font.setStyleHint(QFont::Monospace);
  mono_font.setPixelSize(13);
  raw_item->setFont(mono_font);
  table->setItem(row_pos, 6, raw_item);
  table->scrollToBottom();
}

void MainWindow::apply_skeuo_theme() {
  QString qss = R"(
    QWidget {
      font-family: "Segoe UI", "Helvetica Neue", Arial, sans-serif;
    }
    QMainWindow {
      background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #d8e2ea, stop: 1 #b0bcc6);
    }
    #mainContainer {
      background: transparent;
    }
    #titleLabel {
      font-size: 24px;
      font-weight: bold;
      color: #2c3e50;
    }
    #subtitleLabel {
      font-size: 13px;
      color: #5d6d7e;
      margin-bottom: 6px;
    }
    #badgeLabel {
      background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #fbfbfc, stop: 1 #eaeded);
      border: 1px solid #aab7c4;
      border-radius: 3px;
      padding: 2px 8px;
      font-size: 10px;
      font-weight: bold;
      color: #34495e;
      border-top: 1px solid #ffffff;
      border-bottom: 1px solid #95a5a6;
    }
    #panel {
      background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #fbfbfc, stop: 1 #e3e7eb);
      border: 1px solid #999999;
      border-top: 1px solid #ffffff;
      border-bottom: 2px solid #666666;
      border-radius: 8px;
    }
    #sectionHeader {
      font-size: 15px;
      font-weight: bold;
      color: #2c3e50;
      padding-bottom: 4px;
    }
    #statusLabel {
      color: #2c3e50;
      font-style: italic;
      font-weight: bold;
      margin-top: 4px;
    }
    QPushButton {
      border: 1px solid #7a7a7a;
      border-top: 1px solid #ffffff;
      border-bottom: 2px solid #5a5a5a;
      border-radius: 4px;
      padding: 6px 14px;
      font-weight: bold;
      color: #2b2b2b;
      background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #f0f0f0, stop: 0.4 #e0e0e0, stop: 0.5 #d4d4d4, stop: 1 #c0c0c0);
    }
    QPushButton:hover {
      background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #ffffff, stop: 0.4 #f0f0f0, stop: 0.5 #e4e4e4, stop: 1 #d0d0d0);
    }
    QPushButton:pressed {
      background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #c0c0c0, stop: 1 #e0e0e0);
      border-top: 2px solid #5a5a5a;
      border-bottom: 1px solid #ffffff;
      padding-top: 7px;
      padding-bottom: 5px;
    }
    QPushButton:disabled {
      color: #999999;
      background: #e0e0e0;
      border: 1px solid #bbbbbb;
    }
    QPushButton#primaryButton {
      color: white;
      border: 1px solid #1a5276;
      border-top: 1px solid #85c1e9;
      border-bottom: 2px solid #154360;
      background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #5dade2, stop: 0.4 #3498db, stop: 0.5 #2980b9, stop: 1 #1f618d);
    }
    QPushButton#primaryButton:hover {
      background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #85c1e9, stop: 0.4 #5dade2, stop: 0.5 #3498db, stop: 1 #2980b9);
    }
    QPushButton#primaryButton:pressed {
      background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #1f618d, stop: 1 #3498db);
      border-top: 2px solid #154360;
      border-bottom: 1px solid #85c1e9;
      padding-top: 7px;
      padding-bottom: 5px;
    }
    QPushButton#dangerButton {
      color: white;
      border: 1px solid #7b241c;
      border-top: 1px solid #f1948a;
      border-bottom: 2px solid #641e16;
      background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #e74c3c, stop: 0.4 #cb4335, stop: 0.5 #b03a2e, stop: 1 #943126);
    }
    QPushButton#dangerButton:hover {
      background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #f1948a, stop: 0.4 #e74c3c, stop: 0.5 #cb4335, stop: 1 #b03a2e);
    }
    QPushButton#dangerButton:pressed {
      background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #943126, stop: 1 #cb4335);
      border-top: 2px solid #641e16;
      border-bottom: 1px solid #f1948a;
      padding-top: 7px;
      padding-bottom: 5px;
    }
    QComboBox {
      border: 1px solid #888888;
      border-top: 2px solid #555555;
      border-left: 2px solid #666666;
      border-bottom: 1px solid #ffffff;
      border-right: 1px solid #ffffff;
      border-radius: 4px;
      padding: 4px 8px;
      background: #e8ecef;
      color: #1c2833;
    }
    QComboBox::drop-down {
      subcontrol-origin: padding;
      subcontrol-position: top right;
      width: 24px;
      border-left: 1px solid #aaaaaa;
      background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #f0f0f0, stop: 1 #c0c0c0);
      border-top-right-radius: 3px;
      border-bottom-right-radius: 3px;
    }
    QTableWidget {
      background-color: #fcfcfc;
      alternate-background-color: #eef2f5;
      border: 1px solid #888888;
      border-top: 2px solid #555555;
      border-left: 2px solid #666666;
      border-bottom: 1px solid #ffffff;
      border-right: 1px solid #ffffff;
      border-radius: 4px;
      gridline-color: #d5d8dc;
      color: #1c2833;
      selection-background-color: #d6eaf8;
      selection-color: #1c2833;
    }
    QHeaderView::section:horizontal {
      background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #f0f0f0, stop: 0.4 #e0e0e0, stop: 0.5 #d4d4d4, stop: 1 #c0c0c0);
      color: #1c2833;
      padding: 6px;
      border: 1px solid #888888;
      border-top: 1px solid #ffffff;
      border-left: 1px solid #ffffff;
      border-bottom: 2px solid #555555;
      border-right: 2px solid #555555;
      font-weight: bold;
    }
    QTableCornerButton::section {
      background: transparent;
      border: none;
    }
    QScrollBar:vertical {
      border: 1px solid #aab7c4;
      background: #eaeded;
      width: 14px;
      margin: 0px 0 0px 0;
      border-radius: 2px;
    }
    QScrollBar::handle:vertical {
      background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 #d5d9df, stop: 1 #fbfbfc);
      border: 1px solid #95a5a6;
      min-height: 20px;
      border-radius: 4px;
    }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
      border: 1px solid #95a5a6;
      background: #eaeded;
      height: 14px;
      subcontrol-origin: margin;
    }
    QScrollBar::add-line:vertical {
      subcontrol-position: bottom;
    }
    QScrollBar::sub-line:vertical {
      subcontrol-position: top;
    }
    QProgressBar {
      border: 1px solid #888888;
      border-top: 2px solid #555555;
      border-left: 2px solid #666666;
      border-bottom: 1px solid #ffffff;
      border-right: 1px solid #ffffff;
      border-radius: 6px;
      background: #d0d4d8;
      text-align: center;
      color: #2c3e50;
      font-weight: bold;
    }
    QProgressBar::chunk {
      background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #5dade2, stop: 0.4 #3498db, stop: 0.5 #2980b9, stop: 1 #1f618d);
      border-radius: 4px;
      margin: 2px;
    }
  )";
  setStyleSheet(qss);
}