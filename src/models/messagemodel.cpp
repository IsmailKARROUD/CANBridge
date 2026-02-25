/**
 * @file messagemodel.cpp
 * @brief Implementation of the MessageModel class.
 *
 * Provides data to QTableView for real-time CAN frame display, including
 * formatted timestamps, hex IDs, DLC values, and hex data payloads.
 */

#include "messagemodel.h"
#include <QDateTime>
#include <algorithm>

// ============================================================================
// Constructor
// ============================================================================

MessageModel::MessageModel(QObject *parent) : QAbstractTableModel(parent)
{
}

// ============================================================================
// QAbstractTableModel Interface
// ============================================================================

/**
 * @return Number of captured frames (rows in the table).
 */
int MessageModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return frames.count();
}

/**
 * @return Fixed column count of 4: Timestamp, ID, DLC, Data.
 */
int MessageModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 5;  // Timestamp, Direction, ID, DLC, Data
}

/**
 * Provide display data for the given cell.
 *
 * Column formatting:
 *  - 0 (Timestamp): "hh:mm:ss.zzz" from the frame's millisecond timestamp
 *  - 1 (ID):        "0xNNN" uppercase hex, zero-padded to 3 digits
 *  - 2 (DLC):       Integer 0-8
 *  - 3 (Data):      Space-separated uppercase hex bytes (e.g., "01 FF A0")
 */
QVariant MessageModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= frames.size())
        return QVariant();

    const FrameEntry& entry = frames.at(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0:  // Timestamp
            if (m_timestampFormat == "ms")
                return QString::number(entry.frame.getTimestamp());
            return QDateTime::fromMSecsSinceEpoch(entry.frame.getTimestamp()).toString(m_timestampFormat);
        case 1:  // Direction
            return entry.isSent ? "TX" : "RX";
        case 2:  // CAN ID in hex
            return QString("0x%1").arg(entry.frame.getId(), 3, 16, QChar('0')).toUpper();
        case 3:  // Data Length Code — show actual byte count
        {
            const CANFrame& f = entry.frame;
            if (f.getFrameType() == CanType::FD)
                return CANFrame::fdDlcToBytes(f.getDlc());
            return static_cast<int>(f.getDlc());
        }
        case 4:  // Data payload as hex string
        {
            const QByteArray& d = entry.frame.getData();
            QString dataStr;
            for (int i = 0; i < d.size(); ++i) {
                dataStr += QString("%1 ").arg(static_cast<quint8>(d[i]), 2, 16, QChar('0')).toUpper();
            }
            return dataStr.trimmed();
        }
        }
    }

    return QVariant();
}

/**
 * Provide column header labels for horizontal orientation.
 */
QVariant MessageModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal) {
        switch (section) {
        case 0: return "Timestamp";
        case 1: return "Dir";
        case 2: return "ID";
        case 3: return "DLC";
        case 4: return "Data";
        }
    }

    return QVariant();
}

// ============================================================================
// Frame Management
// ============================================================================

/**
 * Add a new CAN frame to the end of the list.
 * If the list exceeds MAX_FRAMES, the oldest frame (index 0) is removed
 * first to keep memory usage bounded.
 */
void MessageModel::addFrame(const CANFrame& frame, bool isSent)
{
    // Remove oldest frame if at capacity (skip check when unlimited)
    if (m_maxFrames > 0 && frames.size() >= m_maxFrames) {
        beginRemoveRows(QModelIndex(), 0, 0);
        frames.removeFirst();
        endRemoveRows();
    }

    // Append the new frame
    beginInsertRows(QModelIndex(), frames.size(), frames.size());
    frames.append({frame,isSent});
    endInsertRows();
    emit frameAdded();
}

/**
 * Remove all frames and notify the view to refresh.
 */
void MessageModel::clear()
{
    beginResetModel();
    frames.clear();
    endResetModel();
}

/**
 * Set the maximum number of stored frames. Trims oldest entries immediately
 * if the current count exceeds the new limit. Pass 0 for unlimited.
 */
void MessageModel::setMaxFrames(int max)
{
    m_maxFrames = max;

    if (m_maxFrames > 0 && frames.size() > m_maxFrames) {
        int toRemove = frames.size() - m_maxFrames;
        beginRemoveRows(QModelIndex(), 0, toRemove - 1);
        for (int i = 0; i < toRemove; ++i)
            frames.removeFirst();
        endRemoveRows();
    }
}

/**
 * Change the timestamp display format and refresh the timestamp column.
 * @param format Qt date/time format string (e.g., "hh:mm:ss.zzz" or "yyyy-MM-dd hh:mm:ss.zzz").
 */
void MessageModel::setTimestampFormat(const QString& format)
{
    m_timestampFormat = format;
    if (!frames.isEmpty()) {
        emit dataChanged(index(0, 0), index(frames.size() - 1, 0));
    }
}

// ============================================================================
// Direct frame access
// ============================================================================

const CANFrame& MessageModel::frameAt(int row) const
{
    return frames.at(row).frame;
}
