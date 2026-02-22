/**
 * @file CANFrame.cpp
 * @brief Implementation of the CANFrame class.
 *
 * Provides constructors, accessors, and binary serialization/deserialization
 * for CAN frames transmitted over the network.
 */

#include "canframe.h"
#include <QDataStream>
#include <QDateTime>
#include <QIODevice>

// ============================================================================
// Accessors
// ============================================================================

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

// ============================================================================
// Constructors
// ============================================================================

/**
 * Default constructor: all fields zeroed out, data buffer cleared.
 */
CANFrame::CANFrame() : id(0), dlc(0), timestamp(0)
{
    memset(data, 0, sizeof(data));
}

/**
 * Parameterized constructor: creates a frame with a specific ID and payload.
 * The timestamp is set to the current system time in milliseconds since epoch.
 * Only `dlc` bytes are copied from the input data pointer; the rest are zeroed.
 */
CANFrame::CANFrame(quint32 id, quint8 dlc, const quint8* data)
    : id(id), dlc(dlc), timestamp(QDateTime::currentMSecsSinceEpoch())
{
    memset(this->data, 0, sizeof(this->data));
    if (data && dlc <= 8) {
        memcpy(this->data, data, dlc);
    }
}

// ============================================================================
// Serialization / Deserialization
// ============================================================================

/**
 * Serialize this frame into a 21-byte QByteArray.
 *
 * Layout: [id: 4B] [dlc: 1B] [data: 8B] [timestamp: 8B]
 * Uses QDataStream with Qt_6_0 version for cross-platform consistency.
 * All 8 data bytes are always written, regardless of DLC value.
 */
QByteArray CANFrame::serialize() const
{
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);

    stream << id << dlc;
    for (int i = 0; i < 8; ++i) {
        stream << data[i];
    }
    stream << timestamp;

    return bytes;
}

/**
 * Deserialize a 21-byte QByteArray back into a CANFrame.
 * Fields are read in the same order as serialize() writes them.
 */
CANFrame CANFrame::deserialize(const QByteArray& bytes)
{
    CANFrame frame;
    QDataStream stream(bytes);
    stream.setVersion(QDataStream::Qt_6_0);

    stream >> frame.id >> frame.dlc;
    for (int i = 0; i < 8; ++i) {
        stream >> frame.data[i];
    }
    stream >> frame.timestamp;

    return frame;
}
