#ifndef OSCILLOSCOPE_H
#define OSCILLOSCOPE_H

#include <QPainter>
#include <QPainterPath>
#include <QWidget>
#include <cmath>
#include <mutex>
#include <vector>

class WaveformDisplay : public QWidget {
  Q_OBJECT
public:
  explicit WaveformDisplay(QWidget *parent = nullptr) : QWidget(parent) {
    setMinimumSize(640, 340);
    buffer.resize(BUFFER_SIZE, 0.0f);
    ref_buffer.resize(VISIBLE_SAMPLES, 0.0f);
  }

  void pushSamples(const float *samples, size_t count) {
    std::lock_guard<std::mutex> lock(buf_mutex);
    for (size_t i = 0; i < count; ++i) {
      buffer[write_idx] = samples[i];
      write_idx = (write_idx + 1) % BUFFER_SIZE;
    }
  }

  void captureReference() {
    std::lock_guard<std::mutex> lock(buf_mutex);
    size_t start = (write_idx + BUFFER_SIZE - VISIBLE_SAMPLES) % BUFFER_SIZE;
    for (size_t i = 0; i < VISIBLE_SAMPLES; ++i) {
      ref_buffer[i] = buffer[(start + i) % BUFFER_SIZE];
    }
    has_reference = true;
  }

  void clearReference() {
    std::lock_guard<std::mutex> lock(buf_mutex);
    has_reference = false;
  }

  bool isReferenceActive() const { return has_reference; }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();
    int cy = h / 2;

    p.fillRect(rect(), QColor(10, 16, 20));

    p.setPen(QPen(QColor(25, 45, 50), 1, Qt::DotLine));
    int gridX = 12;
    int gridY = 8;
    for (int i = 1; i < gridX; ++i) {
      int x = (w * i) / gridX;
      p.drawLine(x, 0, x, h);
    }
    for (int j = 1; j < gridY; ++j) {
      int y = (h * j) / gridY;
      p.drawLine(0, y, w, y);
    }

    p.setPen(QPen(QColor(35, 75, 80), 1, Qt::SolidLine));
    p.drawLine(0, cy, w, cy);

    std::vector<float> liveData(VISIBLE_SAMPLES);
    std::vector<float> refData(VISIBLE_SAMPLES);
    bool showRef = false;

    {
      std::lock_guard<std::mutex> lock(buf_mutex);
      size_t start = (write_idx + BUFFER_SIZE - VISIBLE_SAMPLES) % BUFFER_SIZE;
      for (size_t i = 0; i < VISIBLE_SAMPLES; ++i) {
        liveData[i] = buffer[(start + i) % BUFFER_SIZE];
      }
      if (has_reference) {
        refData = ref_buffer;
        showRef = true;
      }
    }

    float scaleY = (h / 2.0f) * 0.85f;

    if (showRef) {
      QPainterPath refPath;
      for (size_t i = 0; i < VISIBLE_SAMPLES; ++i) {
        float x = (float)i / (VISIBLE_SAMPLES - 1) * w;
        float y = cy - (refData[i] * scaleY);
        if (i == 0)
          refPath.moveTo(x, y);
        else
          refPath.lineTo(x, y);
      }
      p.setPen(QPen(QColor(0, 140, 255, 60), 6, Qt::SolidLine));
      p.drawPath(refPath);
      p.setPen(QPen(QColor(56, 189, 248, 220), 2, Qt::DashLine, Qt::RoundCap));
      p.drawPath(refPath);
    }

    QPainterPath livePath;
    for (size_t i = 0; i < VISIBLE_SAMPLES; ++i) {
      float x = (float)i / (VISIBLE_SAMPLES - 1) * w;
      float y = cy - (liveData[i] * scaleY);
      if (i == 0)
        livePath.moveTo(x, y);
      else
        livePath.lineTo(x, y);
    }

    p.setPen(QPen(QColor(0, 255, 170, 70), 6, Qt::SolidLine));
    p.drawPath(livePath);
    p.setPen(QPen(QColor(0, 255, 170, 240), 2, Qt::SolidLine, Qt::RoundCap,
                  Qt::RoundJoin));
    p.drawPath(livePath);

    p.setPen(QPen(QColor(40, 70, 80), 2));
    p.drawRect(1, 1, w - 2, h - 2);

    p.setFont(QFont("Monospace", 9));
    if (showRef) {
      p.setPen(QColor(56, 189, 248));
      p.drawText(14, 22, "-- CH1 (AZUL TRACEJADO): SOM ORIGINAL DE REFERENCIA");
      p.setPen(QColor(0, 255, 170));
      p.drawText(14, 38, "—— CH2 (VERDE CONTINUO): SOM MODULADO EM TEMPO REAL");
      p.setPen(QColor(224, 242, 241, 180));
      p.drawText(w - 280, 22, "MODO: DUAL-TRACE COMPARATIVO");
    } else {
      p.setPen(QColor(0, 255, 170, 200));
      p.drawText(
          14, 22,
          "CH1: LIVE UMP AUDIO SINK  |  TIMEBASE: 500us/DIV  |  COUPLING: AC");
      p.setPen(QColor(128, 203, 196, 160));
      p.drawText(w - 330, 22, "DICA: Clique em [Fixar Referencia] p/ Comparar");
    }
  }

private:
  static const size_t BUFFER_SIZE = 4096;
  static const size_t VISIBLE_SAMPLES = 800;
  std::vector<float> buffer;
  std::vector<float> ref_buffer;
  size_t write_idx{0};
  bool has_reference{false};
  std::mutex buf_mutex;
};

#endif