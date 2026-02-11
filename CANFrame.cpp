#include "canframe.h"
#include <QDataStream>
#include <QDateTime>
#include <QIODevice>


// Default constructor: initializes all fields to zero
void CANFrame::setTimestamp(quint64 newTimestamp)
{
    timestamp = newTimestamp;
}

quint64 CANFrame::getTimestamp() const
{
    return timestamp;
}

quint32 CANFrame::getId() const
{
    return id;
}

quint8 CANFrame::getDlc() const
{
    return dlc;
}

quint8 CANFrame::getDataIndex(quint8 i) const
{
    return data[i];
}

CANFrame::CANFrame() : id(0), dlc(0), timestamp(0)
{
    memset(data, 0, sizeof(data));  // Clear data buffer
}

// Parameterized constructor: creates frame with specific ID and payload
CANFrame::CANFrame(quint32 id, quint8 dlc, const quint8* data) : id(id), dlc(dlc), timestamp(QDateTime::currentMSecsSinceEpoch())
{
    memset(this->data, 0, sizeof(this->data));  // Initialize to zero
    if (data && dlc <= 8) {
        memcpy(this->data, data, dlc);  // Copy valid data bytes only
    }
}

// Serialize frame to QByteArray for network transmission
QByteArray CANFrame::serialize() const
{
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);  // Ensure compatibility

    // Write fields in fixed order (protocol contract)
    stream << id << dlc;
    for (int i = 0; i < 8; ++i) {
        stream << data[i];  // Always send 8 bytes (padding for dlc < 8)
    }
    stream << timestamp;

    return bytes;
}

// Deserialize QByteArray back to CANFrame structure
CANFrame CANFrame::deserialize(const QByteArray& bytes)
{
    CANFrame frame;
    QDataStream stream(bytes);
    stream.setVersion(QDataStream::Qt_6_0);  // Match serialization version

    // Read fields in same order as serialize()
    stream >> frame.id >> frame.dlc;
    for (int i = 0; i < 8; ++i) {
        stream >> frame.data[i];
    }
    stream >> frame.timestamp;

    return frame;
}
