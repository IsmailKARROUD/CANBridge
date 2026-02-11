#ifndef CANFRAME_H
#define CANFRAME_H

#include <QtTypes>
 #include <QByteArray>


class CANFrame
{
    quint32 id; // to support 11bit or 29bit ID format
    quint8 dlc; // 4bits
    quint8 data[8]; // Data from 0 to 8 Bytes
    quint64 timestamp;  // Timestamp in milliseconds


public:
    // Constructor
    CANFrame();
    CANFrame(quint32 id, quint8 dlc, const quint8* data);
    // Serialization for network transmission
    QByteArray serialize() const;
    static CANFrame deserialize(const QByteArray& bytes);
    void setTimestamp(quint64 newTimestamp);
    quint64 getTimestamp() const;
    quint32 getId() const;
    quint8 getDlc() const;
    quint8 getDataIndex(quint8 i) const;
};

#endif // CANFRAME_H
