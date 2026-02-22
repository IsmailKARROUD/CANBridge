/**
 * @file CANFrame.h
 * @brief Definition of the CANFrame class representing a CAN bus message.
 *
 * This header defines the core data structure used throughout CANBridge
 * to represent a single CAN (Controller Area Network) frame. It supports
 * both standard (11-bit) and extended (29-bit) CAN identifiers.
 *
 * The frame can be serialized to a 21-byte binary format for network
 * transmission over TCP/IP and deserialized back on the receiving end.
 *
 * Wire format (21 bytes total):
 *   [ID: 4 bytes] [DLC: 1 byte] [Data: 8 bytes] [Timestamp: 8 bytes]
 */

#ifndef CANFRAME_H
#define CANFRAME_H

#include <QtTypes>
#include <QByteArray>

/**
 * @class CANFrame
 * @brief Represents a single CAN bus frame with ID, data payload, and timestamp.
 *
 * A CAN frame consists of:
 *  - An identifier (11-bit standard or 29-bit extended)
 *  - A data length code (DLC) indicating the number of valid data bytes (0-8)
 *  - Up to 8 bytes of payload data
 *  - A millisecond-precision timestamp for logging and analysis
 */
class CANFrame
{
    quint32 id;         ///< CAN identifier (supports both 11-bit and 29-bit formats)
    quint8 dlc;         ///< Data Length Code: number of valid data bytes (0-8)
    quint8 data[8];     ///< Payload data buffer (always 8 bytes, only first `dlc` bytes are valid)
    quint64 timestamp;  ///< Timestamp in milliseconds since epoch (set on frame creation)

public:
    /// Default constructor: initializes all fields to zero.
    CANFrame();

    /**
     * @brief Construct a CAN frame with specific ID and payload.
     * @param id   CAN identifier (11-bit or 29-bit)
     * @param dlc  Number of data bytes (0-8)
     * @param data Pointer to the payload bytes (at most `dlc` bytes are copied)
     *
     * The timestamp is automatically set to the current system time.
     */
    CANFrame(quint32 id, quint8 dlc, const quint8* data);

    /**
     * @brief Serialize the frame into a 21-byte QByteArray for network transmission.
     * @return Binary representation of the frame using QDataStream (Qt 6.0 format).
     */
    QByteArray serialize() const;

    /**
     * @brief Reconstruct a CANFrame from its 21-byte binary representation.
     * @param bytes The serialized data (must be exactly 21 bytes).
     * @return The deserialized CANFrame.
     */
    static CANFrame deserialize(const QByteArray& bytes);

    // --- Accessors ---

    void setTimestamp(quint64 newTimestamp);
    quint64 getTimestamp() const;
    quint32 getId() const;
    void setID(quint32 newId) ;
    quint8 getDlc() const;

    /**
     * @brief Get a single data byte by index.
     * @param i Byte index (0-7). Caller must ensure i < dlc for valid data.
     */
    quint8 getDataIndex(quint8 i) const;
};

#endif // CANFRAME_H
