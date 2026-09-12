#ifndef GUI_H
#define GUI_H

#include <QMainWindow>
#include <QTableWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QThread>
#include <QVariantMap>
#include <RtMidi.h>
#include <memory>
#include "ump.h"

class MidiWorker : public QThread {
    Q_OBJECT
public:
    explicit MidiWorker(RtMidiIn* port, RtMidiOut* out_port = nullptr, QObject *parent = nullptr);
    ~MidiWorker();
    void run() override;

signals:
    void log_signal(const QVariantMap& data);
    void pitch_signal(int value);

private:
    RtMidiIn* m_port;
    RtMidiOut* m_out_port;
    int m_last_note;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void refresh_ports();
    void connect_selected_port();
    void disconnect_port();
    void refresh_ports_out();
    void connect_selected_port_out();
    void disconnect_port_out();
    void simular_pitch_bend();
    void add_table_row(const QVariantMap& data);

private:
    void start_worker();
    void stop_worker();
    void set_status(const QString& text);
    void apply_skeuo_theme();

    QComboBox* port_selector;
    QPushButton* btn_refresh;
    QPushButton* btn_connect;
    QPushButton* btn_disconnect;
    QLabel* status_label;

    QComboBox* port_selector_out;
    QPushButton* btn_refresh_out;
    QPushButton* btn_connect_out;
    QPushButton* btn_disconnect_out;
    QLabel* status_label_out;

    QTableWidget* table;
    QProgressBar* bar;
    QPushButton* btn_simular;
    MidiWorker* worker;
    std::unique_ptr<RtMidiIn> midi_port;
    std::unique_ptr<RtMidiOut> midi_port_out;

    QLabel* lbl_status_con;
    QLabel* lbl_taxa;
    QLabel* lbl_buffer;
    class QTimer* timer_taxa;
    int msg_count;
};

#endif