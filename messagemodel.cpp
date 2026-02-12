#include "messagemodel.h"
#include <QDateTime>

// Constructor
MessageModel::MessageModel(QObject *parent) : QAbstractTableModel(parent)
{
}

// Return number of rows (frames) in model
int MessageModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return frames.count();
}

// Return number of columns (frame fields)
int MessageModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 4;  // Timestamp, ID, DLC, Data
}

// Provide data for each cell
QVariant MessageModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= frames.size())
        return QVariant();

    const CANFrame& frame = frames.at(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0:  // Timestamp
            return QDateTime::fromMSecsSinceEpoch(frame.getTimestamp()).toString("hh:mm:ss.zzz");
        case 1:  // CAN ID (hex)
            return QString("0x%1").arg(frame.getId(), 3, 16, QChar('0')).toUpper();
        case 2:  // DLC
            return frame.getDlc();
        case 3:  // Data bytes (hex)
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

// Provide column headers
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

// Add new frame to model
void MessageModel::addFrame(const CANFrame& frame)
{
    // Prevent memory overflow
    if (frames.size() >= MAX_FRAMES) {
        beginRemoveRows(QModelIndex(), 0, 0);
        frames.removeFirst();  // Remove oldest frame
        endRemoveRows();
    }

    // Insert at end
    beginInsertRows(QModelIndex(), frames.size(), frames.size());
    frames.append(frame);
    endInsertRows();
    emit frameAdded();
}

// Clear all frames
void MessageModel::clear()
{
    beginResetModel();
    frames.clear();
    endResetModel();
}
