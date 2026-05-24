#include "gui.h"
#include "converter.h"
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QPainter>
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
  resize(1550, 850);
  setMinimumSize(1450, 760);

  QWidget *container = new QWidget(this);
  container->setObjectName("mainContainer");
  QVBoxLayout *layout = new QVBoxLayout(container);
  layout->setContentsMargins(0, 0, 0, 15);
  layout->setSpacing(12);

  QFrame *topHeader = new QFrame(this);
  topHeader->setObjectName("topHeader");
  QVBoxLayout *headerLayout = new QVBoxLayout(topHeader);
  headerLayout->setContentsMargins(0, 12, 0, 12);
  headerLayout->setSpacing(4);

  QLabel *title_label = new QLabel("MIDI2Bridge", this);
  title_label->setObjectName("titleLabel");
  title_label->setAlignment(Qt::AlignCenter);

  QLabel *subtitle_label =
      new QLabel("CONVERSOR E ANALISADOR MIDI 1.0 -> MIDI 2.0 UMP", this);
  subtitle_label->setObjectName("subtitleLabel");
  subtitle_label->setAlignment(Qt::AlignCenter);

  headerLayout->addWidget(title_label);
  headerLayout->addWidget(subtitle_label);
  layout->addWidget(topHeader);

  QHBoxLayout *main_split = new QHBoxLayout();
  main_split->setContentsMargins(15, 0, 15, 0);
  layout->addLayout(main_split, 1);

  QWidget *left_widget = new QWidget(this);
  left_widget->setFixedWidth(380);
  QVBoxLayout *left_layout = new QVBoxLayout(left_widget);
  left_layout->setContentsMargins(0, 0, 0, 0);
  left_layout->setSpacing(12);

  QFrame *input_panel = new QFrame(this);
  input_panel->setObjectName("panel");
  input_panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
  QVBoxLayout *input_layout = new QVBoxLayout(input_panel);
  input_layout->setContentsMargins(15, 12, 15, 12);
  input_layout->setSpacing(10);

  port_selector = new QComboBox(this);
  btn_refresh = new QPushButton("Atualizar", this);
  btn_refresh->setObjectName("secondaryButton");
  btn_refresh->setIcon(QIcon(":/assets/icons/refresh.svg"));
  btn_refresh->setAutoDefault(false);
  btn_refresh->setDefault(false);
  btn_refresh->setFocusPolicy(Qt::NoFocus);

  btn_connect = new QPushButton("Conectar", this);
  btn_connect->setObjectName("primaryButton");
  btn_connect->setIcon(QIcon(":/assets/icons/cable.svg"));
  btn_connect->setIconSize(QSize(16, 16));
  btn_connect->setFixedHeight(30);
  btn_connect->setAutoDefault(false);
  btn_connect->setDefault(false);
  btn_connect->setFocusPolicy(Qt::NoFocus);

  btn_disconnect = new QPushButton("Desconectar", this);
  btn_disconnect->setObjectName("dangerButton");
  btn_disconnect->setIcon(QIcon(":/assets/icons/power_off.svg"));
  btn_disconnect->setIconSize(QSize(16, 16));
  btn_disconnect->setFixedHeight(30);
  btn_disconnect->setAutoDefault(false);
  btn_disconnect->setDefault(false);
  btn_disconnect->setFocusPolicy(Qt::NoFocus);

  status_label = new QLabel(this);
  status_label->setObjectName("statusLabel");

  QHBoxLayout *status_layout = new QHBoxLayout();
  QFrame *led_status = new QFrame(this);
  led_status->setObjectName("statusLed");
  led_status->setFixedSize(10, 10);
  status_layout->addWidget(led_status);
  status_layout->addWidget(status_label, 1);
  status_layout->setContentsMargins(0, 0, 0, 0);

  QHBoxLayout *btn_layout = new QHBoxLayout();
  btn_layout->addWidget(btn_connect, 1);
  btn_layout->addWidget(btn_disconnect, 1);

  QHBoxLayout *header_entrada = new QHBoxLayout();
  QLabel *icon_entrada = new QLabel(this);
  icon_entrada->setPixmap(
      QIcon(":/assets/icons/settings_input_component.svg").pixmap(16, 16));
  QLabel *lbl_entrada = new QLabel("ENTRADA MIDI", this);
  lbl_entrada->setObjectName("sectionHeader");
  header_entrada->addWidget(icon_entrada);
  header_entrada->addWidget(lbl_entrada);
  header_entrada->addStretch();

  input_layout->addLayout(header_entrada);
  input_layout->addWidget(port_selector);
  input_layout->addWidget(btn_refresh);
  input_layout->addLayout(btn_layout);
  input_layout->addLayout(status_layout);

  left_layout->addWidget(input_panel);

  QFrame *pitch_panel = new QFrame(this);
  pitch_panel->setObjectName("panel");
  pitch_panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
  QVBoxLayout *pitch_layout = new QVBoxLayout(pitch_panel);
  pitch_layout->setContentsMargins(15, 12, 15, 12);

  bar = new QProgressBar(this);
  bar->setObjectName("pitchBar");
  btn_simular = new QPushButton("Simular Pitch Bend", this);
  btn_simular->setObjectName("secondaryButton");
  btn_simular->setIcon(QIcon(":/assets/icons/speed.svg"));
  btn_simular->setAutoDefault(false);
  btn_simular->setDefault(false);
  btn_simular->setFocusPolicy(Qt::NoFocus);

  QHBoxLayout *header_pitch = new QHBoxLayout();
  QLabel *icon_pitch = new QLabel(this);
  icon_pitch->setPixmap(
      QIcon(":/assets/icons/linear_scale.svg").pixmap(16, 16));
  QLabel *lbl_pitch = new QLabel("RESOLUÇÃO PITCH BEND (32-BIT)", this);
  lbl_pitch->setObjectName("sectionHeader");
  header_pitch->addWidget(icon_pitch);
  header_pitch->addWidget(lbl_pitch);
  header_pitch->addStretch();

  pitch_layout->addLayout(header_pitch);
  pitch_layout->addWidget(bar);
  pitch_layout->addWidget(btn_simular);

  left_layout->addWidget(pitch_panel);
  left_layout->addStretch();

  main_split->addWidget(left_widget, 0);

  QWidget *right_widget = new QWidget(this);
  QVBoxLayout *right_layout = new QVBoxLayout(right_widget);
  right_layout->setContentsMargins(0, 0, 0, 0);
  right_layout->setSpacing(12);

  QFrame *table_panel = new QFrame(this);
  table_panel->setObjectName("panel");
  table_panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  QVBoxLayout *table_layout = new QVBoxLayout(table_panel);
  table_layout->setContentsMargins(15, 12, 15, 12);

  table = new QTableWidget(0, 8, this);
  table->setHorizontalHeaderLabels({"#", "Mensagem", "Ch", "Alvo",
                                    "Valor Original", "Valor Convertido",
                                    "UMP Word 0", "UMP Word 1"});
  table->verticalHeader()->setVisible(false);
  table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  table->horizontalHeader()->setStretchLastSection(true);
  table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  table->setAlternatingRowColors(true);

  table->setColumnWidth(0, 45);
  table->setColumnWidth(1, 140);
  table->setColumnWidth(2, 45);
  table->setColumnWidth(3, 180);
  table->setColumnWidth(4, 170);
  table->setColumnWidth(5, 170);
  table->setColumnWidth(6, 150);
  table->setColumnWidth(7, 150);

  table->horizontalHeader()->setMinimumHeight(34);
  table->verticalHeader()->setDefaultSectionSize(32);
  table->setMinimumHeight(320);
  table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  QFont tableFont = table->font();
  tableFont.setPixelSize(13);
  table->setFont(tableFont);

  QHBoxLayout *table_header_layout = new QHBoxLayout();
  QLabel *icon_table = new QLabel(this);
  icon_table->setPixmap(QIcon(":/assets/icons/analytics.svg").pixmap(16, 16));
  QLabel *lbl_table =
      new QLabel("ANALISADOR DE PACOTES UMP EM TEMPO REAL", this);
  lbl_table->setObjectName("sectionHeader");

  QLabel *badge =
      new QLabel("<span style='color: #2563eb;'>●</span> Monitor Ativo", this);
  badge->setObjectName("badgeLabel");
  badge->setAlignment(Qt::AlignCenter);

  table_header_layout->addWidget(icon_table);
  table_header_layout->addWidget(lbl_table);
  table_header_layout->addStretch();
  table_header_layout->addWidget(badge);

  QFrame *h_line = new QFrame();
  h_line->setFrameShape(QFrame::HLine);
  h_line->setFrameShadow(QFrame::Sunken);
  h_line->setStyleSheet(
      "border: 1px solid #c0c0c0; border-bottom: 1px solid #ffffff;");

  table_layout->addLayout(table_header_layout);
  table_layout->addWidget(h_line);
  table_layout->addWidget(table, 1);

  right_layout->addWidget(table_panel, 1);

  QFrame *footer_panel = new QFrame(this);
  footer_panel->setObjectName("panel");
  footer_panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
  QHBoxLayout *footer_layout = new QHBoxLayout(footer_panel);
  footer_layout->setContentsMargins(15, 8, 15, 8);

  QLabel *icon_con = new QLabel(this);
  icon_con->setPixmap(QIcon(":/assets/icons/monitor.svg").pixmap(14, 14));
  QLabel *lbl_status_con = new QLabel(
      "<span style='color: #2563eb;'>●</span> Status: Conectado", this);
  lbl_status_con->setObjectName("footerLabel");

  QLabel *icon_taxa = new QLabel(this);
  icon_taxa->setPixmap(
      QIcon(":/assets/icons/arrow_right_alt.svg").pixmap(14, 14));
  QLabel *lbl_taxa = new QLabel("Taxa UMP: 0 msgs/s", this);
  lbl_taxa->setObjectName("footerLabel");

  QLabel *icon_buf = new QLabel(this);
  icon_buf->setPixmap(QIcon(":/assets/icons/hard_drive.svg").pixmap(14, 14));
  QLabel *lbl_buffer = new QLabel("Buffer: 0/1024", this);
  lbl_buffer->setObjectName("footerLabel");

  footer_layout->addWidget(icon_con);
  footer_layout->addWidget(lbl_status_con);
  footer_layout->addStretch();
  footer_layout->addWidget(icon_taxa);
  footer_layout->addWidget(lbl_taxa);
  footer_layout->addStretch();
  footer_layout->addWidget(icon_buf);
  footer_layout->addWidget(lbl_buffer);

  right_layout->addWidget(footer_panel);

  main_split->addWidget(right_widget, 1);
  main_split->setStretch(0, 0);
  main_split->setStretch(1, 1);

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

  QTableWidgetItem *idx_item =
      new QTableWidgetItem(QString::number(row_pos + 1));
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

  QTableWidgetItem *w0_item =
      new QTableWidgetItem(data.value("raw_w1").toString());
  QTableWidgetItem *w1_item =
      new QTableWidgetItem(data.value("raw_w2").toString());

  QFont mono_font("Consolas");
  mono_font.setStyleHint(QFont::Monospace);
  mono_font.setPixelSize(13);
  w0_item->setFont(mono_font);
  w1_item->setFont(mono_font);

  table->setItem(row_pos, 6, w0_item);
  table->setItem(row_pos, 7, w1_item);
  table->scrollToBottom();
}

void MainWindow::apply_skeuo_theme() {
  QString qss = R"(
    QWidget {
      font-family: "Segoe UI", "Helvetica Neue", Arial, sans-serif;
    }
    QMainWindow {
      background: #d5dbe2;
    }
    #mainContainer {
      background: transparent;
    }
    #topHeader {
      background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #e8eef5, stop: 1 #c3c6d7);
      border-bottom: 2px solid #737686;
    }
    #titleLabel {
      font-size: 28px;
      font-weight: bold;
      color: #161c21;
      letter-spacing: 1px;
    }
    #subtitleLabel {
      font-size: 13px;
      color: #555f6d;
      letter-spacing: 1px;
    }
    #badgeLabel {
      background: #e8eef5;
      border: 1px solid #c3c6d7;
      border-radius: 3px;
      padding: 2px 8px;
      font-size: 10px;
      font-weight: bold;
      color: #161c21;
    }
    #panel {
      background: #f6faff;
      border: 1px solid #737686;
      border-top: 1px solid #ffffff;
      border-bottom: 2px solid #737686;
      border-radius: 8px;
    }
    #sectionHeader {
      font-size: 13px;
      font-weight: bold;
      color: #161c21;
      padding-bottom: 4px;
    }
    #statusLed {
      background: qradialgradient(cx: 0.35, cy: 0.35, radius: 0.8, fx: 0.35, fy: 0.35, stop: 0 #8fd3ff, stop: 0.45 #2563eb, stop: 1 #004ac6);
      border: 1px solid #003ea8;
      border-radius: 5px;
      min-width: 10px;
      min-height: 10px;
      max-width: 10px;
      max-height: 10px;
    }
    #statusLabel {
      color: #161c21;
      font-style: italic;
      font-weight: bold;
      margin-top: 0px;
    }
    #footerLabel {
      color: #161c21;
      font-size: 12px;
      font-weight: bold;
    }
    QPushButton {
      outline: none;
      border: 1px solid #737686;
      border-top: 1px solid #ffffff;
      border-bottom: 2px solid #555f6d;
      border-radius: 4px;
      padding: 6px 12px;
      font-weight: bold;
      color: #161c21;
      background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #ffffff, stop: 1 #dde3ea);
    }
    QPushButton:focus {
      outline: none;
    }
    QPushButton:hover {
      background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #ffffff, stop: 1 #e8eef5);
    }
    QPushButton:pressed {
      background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #dde3ea, stop: 1 #c3c6d7);
      border-top: 2px solid #737686;
      border-bottom: 1px solid #ffffff;
      padding: 7px 12px 5px 12px;
    }
    QPushButton:disabled {
      color: #999999;
      background: #e0e0e0;
      border: 1px solid #bbbbbb;
    }
    QPushButton#primaryButton {
      color: white;
      border: 1px solid #004ac6;
      border-top: 1px solid #85c1e9;
      border-bottom: 2px solid #003699;
      background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #5dade2, stop: 0.4 #2563eb, stop: 1 #004ac6);
    }
    QPushButton#primaryButton:hover {
      background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #85c1e9, stop: 0.4 #5dade2, stop: 1 #2563eb);
    }
    QPushButton#primaryButton:pressed {
      background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #004ac6, stop: 1 #2563eb);
      border-top: 2px solid #003699;
      border-bottom: 1px solid #85c1e9;
      padding: 7px 12px 5px 12px;
    }
    QPushButton#dangerButton {
      color: white;
      font-weight: bold;
      border: 1px solid #8b0000;
      border-top: 1px solid #f1948a;
      border-radius: 4px;
      padding: 6px 12px;
      background: #c71f1f;
    }
    QPushButton#dangerButton:hover {
      color: white;
      border: 1px solid #8b0000;
      border-top: 1px solid #f1948a;
      padding: 6px 12px;
      background: #d52022;
    }
    QPushButton#dangerButton:pressed {
      color: white;
      border: 1px solid #8b0000;
      border-top: 1px solid #5f0006;
      padding: 6px 12px;
      background: #9f000e;
    }
    QPushButton#dangerButton:focus {
      color: white;
      border: 1px solid #8b0000;
      border-top: 1px solid #f1948a;
      padding: 6px 12px;
    }
    QPushButton#dangerButton:disabled {
      color: #eeeeee;
      border: 1px solid #999999;
      padding: 6px 12px;
      background: #b0b0b0;
    }
    QComboBox {
      border: 1px solid #c3c6d7;
      border-top: 2px solid #737686;
      border-radius: 4px;
      padding: 6px 36px 6px 8px;
      background: #e8eef5;
      color: #161c21;
      font-family: "JetBrains Mono", Consolas, monospace;
    }
    QComboBox::drop-down {
      subcontrol-origin: padding;
      subcontrol-position: top right;
      width: 28px;
      border-left: 1px solid #c3c6d7;
      background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #f0f0f0, stop: 1 #c0c0c0);
      border-top-right-radius: 3px;
      border-bottom-right-radius: 3px;
    }
    QComboBox::down-arrow {
      image: url(:/assets/icons/arrow_drop_down.svg);
      width: 18px;
      height: 18px;
      margin-right: 4px;
    }
    QTableWidget {
      background-color: #f0f4f8;
      alternate-background-color: #eef4fb;
      border: 1px solid #c3c6d7;
      border-top: 2px solid #737686;
      border-radius: 4px;
      gridline-color: #dde3ea;
      color: #161c21;
      selection-background-color: #d6eaf8;
      selection-color: #161c21;
    }
    QHeaderView::section:horizontal {
      background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #eef4fb, stop: 1 #dde3ea);
      color: #161c21;
      padding: 6px;
      border: 1px solid #c3c6d7;
      border-bottom: 2px solid #737686;
      font-weight: bold;
    }
    QTableCornerButton::section {
      background: transparent;
      border: none;
    }
    QScrollBar:vertical {
      border: 1px solid #c3c6d7;
      background: #e8eef5;
      width: 14px;
      margin: 0px 0 0px 0;
      border-radius: 2px;
    }
    QScrollBar::handle:vertical {
      background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 #dde3ea, stop: 1 #f6faff);
      border: 1px solid #737686;
      min-height: 20px;
      border-radius: 4px;
    }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
      border: 1px solid #c3c6d7;
      background: #e8eef5;
      height: 14px;
      subcontrol-origin: margin;
    }
    QProgressBar {
      border: 1px solid #737686;
      border-top: 2px solid #434655;
      border-bottom: 1px solid #ffffff;
      border-radius: 6px;
      background: #d5dbe2;
      text-align: center;
      color: #161c21;
      font-weight: bold;
      min-height: 28px;
    }
    QProgressBar::chunk {
      background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #5dade2, stop: 0.4 #2563eb, stop: 1 #004ac6);
      border-radius: 4px;
      margin: 2px;
    }
  )";
  setStyleSheet(qss);
}
