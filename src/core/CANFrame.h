/**
 * @file CANFrame.h
 * @brief Definition of the CANFrame class representing a CAN bus message.
 *
 * Supports CAN Classic, CAN-FD, and CAN-XL bus types with both Standard
 * (11-bit) and Extended (29-bit) identifiers.
 *
 * Wire format (18-byte header + variable data):
 *   [CanType:   1 byte ]  0=Classic, 1=FD, 2=XL
 *   [IdFormat:  1 byte ]  0=Standard, 1=Extended
 *   [ID:        4 bytes]  quint32
 *   [DLC:       2 bytes]  quint16 (DLC code per spec)
 *   [DataLen:   2 bytes]  quint16 (actual bytes following)
 *   [Timestamp: 8 bytes]  quint64
 *   [Data:   DataLen bytes]
 * Total = 18 + DataLen bytes
 */

#ifndef CANFRAME_H
#define CANFRAME_H

#include <QtTypes>
#include <QByteArray>

/// CAN bus type — determines maximum data payload and DLC encoding
enum class CanType  { Classic, FD, XL };

/// CAN identifier format — 11-bit Standard or 29-bit Extended
enum class IdFormat { Standard, Extended };

/**
 * @class CANFrame
 * @brief Represents a single CAN bus frame with ID, data payload, and timestamp.
 */
class CANFrame
{
    quint32  id;            ///< CAN identifier
    quint16  dlc;           ///< DLC code (for FD: 0-15 per spec; for Classic/XL: actual byte count)
    QByteArray data;        ///< Payload data (variable size)
    quint64  timestamp;     ///< Milliseconds since epoch
    CanType  frameType;     ///< Bus type this frame was captured/created on
    IdFormat idFormat;      ///< ID format (Standard 11-bit or Extended 29-bit)

public:
    // -------------------------------------------------------------------------
    // Constants and DLC helpers
    // -------------------------------------------------------------------------

    static constexpr quint32 ID_MAX_STANDARD = 0x7FF;
    static constexpr quint32 ID_MAX_EXTENDED = 0x1FFFFFFF;

    /// CAN-FD DLC table: index = DLC code (0-15), value = actual byte count
    static constexpr int FD_DLC_TABLE[16] = {0,1,2,3,4,5,6,7,8,12,16,20,24,32,48,64};

    /// Returns the smallest FD DLC code whose actual byte count is >= @p bytes.
    static int bytesToFdDlc(int bytes);

    /// Returns the actual byte count for a given FD DLC code (0-15).
    static int fdDlcToBytes(int dlcCode);

    // -------------------------------------------------------------------------
    // Constructors
    // -------------------------------------------------------------------------

    /// Default constructor: all fields zeroed, Classic/Standard type.
    CANFrame();

    /**
     * @brief Construct a CAN frame with specific ID, DLC code, and payload.
     * @param id        CAN identifier
     * @param dlc       DLC code (byte count for Classic/XL; DLC code 0-15 for FD)
     * @param data      Payload bytes
     * @param type      Bus type (Classic, FD, or XL)
     * @param fmt       ID format (Standard or Extended)
     *
     * Timestamp is set to the current system time.
     */
    CANFrame(quint32 id, quint16 dlc, const QByteArray& data,
             CanType type = CanType::Classic, IdFormat fmt = IdFormat::Standard);

    // -------------------------------------------------------------------------
    // Serialization
    // -------------------------------------------------------------------------

    /// Serialize to 18-byte header + DataLen data bytes.
    QByteArray serialize() const;

    /// Reconstruct a CANFrame from its serialized form.
    static CANFrame deserialize(const QByteArray& bytes);

    // -------------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------------

    void     setTimestamp(quint64 newTimestamp);
    quint64  getTimestamp() const;
    quint32  getId() const;
    void     setID(quint32 newId);
    quint16  getDlc() const;
    CanType  getFrameType() const;
    IdFormat getIdFormat() const;

    /// Returns a single payload byte by index. Caller must ensure i < data.size().
    quint8 getDataIndex(int i) const;

    /// Returns the full payload as a QByteArray.
    const QByteArray& getData() const;
};

#endif // CANFRAME_H
