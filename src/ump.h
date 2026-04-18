#ifndef UMP_H
#define UMP_H

#include <cstdint>
#include <QString>
#include <QMap>

class UMPMessage {
public:
    uint32_t word1;
    uint32_t word2;

    UMPMessage(uint32_t w1, uint32_t w2 = 0) : word1(w1), word2(w2) {}

    uint64_t ump64() const { return ((uint64_t)word1 << 32) | word2; }

    QMap<QString, QString> analyze() const {
        QMap<QString, QString> data;
        uint8_t mt = (word1 >> 28) & 0xF;
        uint8_t status = (word1 >> 20) & 0xF;
        uint8_t channel = (word1 >> 16) & 0xF;

        data["channel"] = QString::number(channel);
        data["raw_w1"] = QString("0x%1").arg(word1, 8, 16, QChar('0')).toUpper();
        data["raw_w2"] = QString("0x%1").arg(word2, 8, 16, QChar('0')).toUpper();

        if (mt == 4) {
            if (status == 0x9) {
                data["type"] = "Note On";
                data["target"] = QString("Nota %1").arg((word2 >> 24) & 0xFF);
                data["value"] = QString("Vel: %1").arg(word2 & 0xFFFF);
            } else if (status == 0x8) {
                data["type"] = "Note Off";
                data["target"] = QString("Nota %1").arg((word2 >> 24) & 0xFF);
                data["value"] = QString("Vel: %1").arg(word2 & 0xFFFF);
            } else if (status == 0xE) {
                data["type"] = "Pitch Bend";
                data["target"] = "-";
                data["value"] = QString("Val: %1").arg(word2 & 0xFFFFFFFF);
            } else if (status == 0xB) {
                data["type"] = "Control Change";
                uint8_t idx = (word1 >> 8) & 0x7F;
                data["target"] = QString("CC %1").arg(idx);
                data["value"] = QString("Val: %1").arg(word2 & 0xFFFFFFFF);
            } else if (status == 0xA) {
                data["type"] = "Per-Note Ctrl";
                uint8_t nota = (word1 >> 8) & 0xFF;
                uint8_t idx = word1 & 0xFF;
                data["target"] = QString("Nota %1 (Idx %2)").arg(nota).arg(idx);
                data["value"] = QString("Val: %1").arg(word2 & 0xFFFFFFFF);
            } else {
                data["type"] = QString("Desconhecido (0x%1)").arg(status, 0, 16);
                data["target"] = "-";
                data["value"] = "-";
            }
        } else {
            data["type"] = QString("MT Desconhecido (%1)").arg(mt);
            data["target"] = "-";
            data["value"] = "-";
        }
        return data;
    }
};

inline UMPMessage create_midi2_note_on(uint8_t note, uint16_t vel16, uint8_t channel=0, uint8_t group=0) {
    uint32_t word1 = (0x4 << 28) | (group << 24) | (0x9 << 20) | (channel << 16);
    uint32_t word2 = (note << 24) | vel16;
    return UMPMessage(word1, word2);
}

inline UMPMessage create_midi2_note_off(uint8_t note, uint16_t vel16, uint8_t channel=0, uint8_t group=0) {
    uint32_t word1 = (0x4 << 28) | (group << 24) | (0x8 << 20) | (channel << 16);
    uint32_t word2 = (note << 24) | vel16;
    return UMPMessage(word1, word2);
}

inline UMPMessage create_midi2_pitch_bend(uint32_t pitch32, uint8_t channel=0, uint8_t group=0) {
    uint32_t word1 = (0x4 << 28) | (group << 24) | (0xE << 20) | (channel << 16);
    return UMPMessage(word1, pitch32);
}

inline UMPMessage create_midi2_control_change(uint8_t index, uint32_t val32, uint8_t channel=0, uint8_t group=0) {
    uint32_t word1 = (0x4 << 28) | (group << 24) | (0xB << 20) | (channel << 16) | (index << 8);
    return UMPMessage(word1, val32);
}

inline UMPMessage create_midi2_per_note_controller(uint8_t note, uint8_t index, uint32_t val32, uint8_t channel=0, uint8_t group=0) {
    uint32_t word1 = (0x4 << 28) | (group << 24) | (0xA << 20) | (channel << 16) | (note << 8) | index;
    return UMPMessage(word1, val32);
}

#endif