/**
 * @file CANFrame.cpp
 * @brief Implementation of the CANFrame class.
 *
 * Wire format (18-byte header + variable data):
 *   [CanType: 1B] [IdFormat: 1B] [ID: 4B] [DLC: 2B] [DataLen: 2B] [Timestamp: 8B] [Data: DataLen B]
 */

#include "CANFrame.h"
#include <QDataStream>
#include <QDateTime>
#include <QIODevice>

// ============================================================================
// DLC Helpers
// ============================================================================

/**
 * Returns the smallest FD DLC code (0-15) whose corresponding actual byte count
 * in FD_DLC_TABLE is >= @p bytes. If bytes > 64 (max FD payload), returns 15.
 */
int CANFrame::bytesToFdDlc(int bytes)
{
    for (int code = 0; code < 16; ++code) {
        if (FD_DLC_TABLE[code] >= bytes)
            return code;
    }
    return 15;  // Max FD DLC code (64 bytes)
}

/**
 * Returns the actual byte count for a given FD DLC code (0-15).
 * Clamps out-of-range codes to the valid range.
 */
int CANFrame::fdDlcToBytes(int dlcCode)
{
    if (dlcCode < 0)  return 0;
    if (dlcCode > 15) return 64;
    return FD_DLC_TABLE[dlcCode];
}

// ============================================================================
// Accessors
// ============================================================================

void CANFrame::setTimestamp(quint64 newTimestamp) { timestamp = newTimestamp; }
quint64  CANFrame::getTimestamp() const           { return timestamp; }
quint32  CANFrame::getId() const                  { return id; }
void     CANFrame::setID(quint32 newId)           { id = newId; }
quint16  CANFrame::getDlc() const                 { return dlc; }
CanType  CANFrame::getFrameType() const           { return frameType; }
IdFormat CANFrame::getIdFormat() const            { return idFormat; }

quint8 CANFrame::getDataIndex(int i) const
{
    return static_cast<quint8>(data[i]);
}

const QByteArray& CANFrame::getData() const
{
    return data;
}

// ============================================================================
// Constructors
// ============================================================================

CANFrame::CANFrame()
    : id(0), dlc(0), timestamp(0)
    , frameType(CanType::Classic), idFormat(IdFormat::Standard)
{
}

CANFrame::CANFrame(quint32 id, quint16 dlc, const QByteArray& data,
                   CanType type, IdFormat fmt)
    : id(id), dlc(dlc), data(data)
    , timestamp(QDateTime::currentMSecsSinceEpoch())
    , frameType(type), idFormat(fmt)
{
}

// ============================================================================
// Serialization / Deserialization
// ============================================================================

/**
 * Serialize this frame into (18 + DataLen) bytes.
 *
 * Header layout (18 bytes):
 *   [CanType: 1B][IdFormat: 1B][ID: 4B][DLC: 2B][DataLen: 2B][Timestamp: 8B]
 * Followed by DataLen raw data bytes.
 */
QByteArray CANFrame::serialize() const
{
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);

    stream << static_cast<quint8>(frameType);        // 1 byte
    stream << static_cast<quint8>(idFormat);         // 1 byte
    stream << id;                                    // 4 bytes
    stream << dlc;                                   // 2 bytes
    stream << static_cast<quint16>(data.size());     // 2 bytes (DataLen)
    stream << timestamp;                             // 8 bytes
    // Header total: 18 bytes

    bytes.append(data);                              // DataLen bytes

    return bytes;
}

/**
 * Reconstruct a CANFrame from its serialized binary form.
 * @p bytes must contain at least 18 bytes (the header); data is taken from
 * the bytes following the header up to DataLen.
 */
CANFrame CANFrame::deserialize(const QByteArray& bytes)
{
    CANFrame frame;

    QDataStream stream(bytes);
    stream.setVersion(QDataStream::Qt_6_0);

    quint8  ct, idf;
    quint16 dataLen;

    stream >> ct >> idf >> frame.id >> frame.dlc >> dataLen >> frame.timestamp;

    frame.frameType = static_cast<CanType>(ct);
    frame.idFormat  = static_cast<IdFormat>(idf);

    constexpr int HEADER_SIZE = 18;  // 1 + 1 + 4 + 1 + 2 + 8 bytes
    frame.data      = bytes.mid(18, dataLen); // Start after header to extract data payload

    return frame;
}
