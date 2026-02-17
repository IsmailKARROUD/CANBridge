/**
 * @file messagemodel.cpp
 * @brief Implementation of the MessageModel class.
 *
 * Provides data to QTableView for real-time CAN frame display, including
 * formatted timestamps, hex IDs, DLC values, and hex data payloads.
 */

#include "messagemodel.h"
#include <QDateTime>

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
    return 4;
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

    const CANFrame& frame = frames.at(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0:  // Timestamp
            return QDateTime::fromMSecsSinceEpoch(frame.getTimestamp()).toString("hh:mm:ss.zzz");
        case 1:  // CAN ID in hex
            return QString("0x%1").arg(frame.getId(), 3, 16, QChar('0')).toUpper();
        case 2:  // Data Length Code
            return frame.getDlc();
        case 3:  // Data payload as hex string
        {
            QString dataStr;
            for (int i = 0; i < frame.getDlc(); ++i) {
                dataStr += QString("%1 ").arg(frame.getDataIndex(i), 2, 16, QChar('0')).toUpper();
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
        case 1: return "ID";
        case 2: return "DLC";
        case 3: return "Data";
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
void MessageModel::addFrame(const CANFrame& frame)
{
    // Remove oldest frame if at capacity
    if (frames.size() >= MAX_FRAMES) {
        beginRemoveRows(QModelIndex(), 0, 0);
        frames.removeFirst();
        endRemoveRows();
    }

    // Append the new frame
    beginInsertRows(QModelIndex(), frames.size(), frames.size());
    frames.append(frame);
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
