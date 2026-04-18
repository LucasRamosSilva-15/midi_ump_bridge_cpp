#ifndef GUI_H
#define GUI_H

#include <QMainWindow>
#include <QTableWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QLabel>
#include <QThread>
#include <QVariantMap>
#include <RtMidi.h>
#include "ump.h"

class MidiWorker : public QThread {
    Q_OBJECT
public:
    explicit MidiWorker(RtMidiIn* port, QObject *parent = nullptr);
    ~MidiWorker();
    void run() override;

signals:
    void log_signal(const QVariantMap& data);
    void pitch_signal(int value);

private:
    RtMidiIn* m_port;
    int m_last_note;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(RtMidiIn* port, QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void simular_pitch_bend();
    void add_table_row(const QVariantMap& data);

private:
    QTableWidget* table;
    QProgressBar* bar;
    QPushButton* btn_simular;
    MidiWorker* worker;
};

#endif