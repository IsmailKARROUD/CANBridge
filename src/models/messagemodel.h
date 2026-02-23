/**
 * @file messagemodel.h
 * @brief Definition of the MessageModel class for displaying CAN frames in a table.
 *
 * MessageModel is a Qt table model that stores received CAN frames and
 * provides them to a QTableView for real-time display. It exposes four
 * columns: Timestamp, ID, DLC, and Data.
 *
 * A maximum frame limit (MAX_FRAMES) prevents unbounded memory growth
 * during long capture sessions.
 */

#ifndef MESSAGEMODEL_H
#define MESSAGEMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include "CANFrame.h"

/**
 * @class MessageModel
 * @brief Qt table model for storing and displaying captured CAN frames.
 *
 * Columns:
 *  - 0: Timestamp (formatted as hh:mm:ss.zzz)
 *  - 1: CAN ID (displayed as uppercase hex, e.g., 0x1A3)
 *  - 2: DLC (data length code, 0-8)
 *  - 3: Data (hex bytes separated by spaces, e.g., "01 FF A0")
 *
 * When the frame count exceeds MAX_FRAMES, the oldest frame is automatically
 * removed to keep memory usage bounded.
 */
class MessageModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit MessageModel(QObject *parent = nullptr);

    // --- QAbstractTableModel interface ---
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    /// Change the timestamp display format and refresh the timestamp column.
    void setTimestampFormat(const QString& format);

    /**
     * @brief Set the maximum number of frames to keep in the model.
     * @param max Upper limit. Use 0 for unlimited.
     * If the current frame count exceeds the new limit, oldest frames are trimmed.
     */
    void setMaxFrames(int max);

public slots:
    /**
     * @brief Append a new CAN frame to the model.
     * @param frame The frame to add. If MAX_FRAMES is exceeded, the oldest frame is removed.
     *
     * Emits frameAdded() after insertion so the UI can update counters.
     */
    void addFrame(const CANFrame& frame, bool isSent);

    /// Remove all frames from the model and reset the view.
    void clear();

signals:
    /// Emitted after a frame is added, used to update the message count label.
    void frameAdded();

private:
    int m_maxFrames = 10000;                     ///< Upper limit to prevent memory exhaustion (0 = unlimited)
    QString m_timestampFormat = "hh:mm:ss.zzz"; ///< Current timestamp display format
    struct FrameEntry {
        CANFrame frame;
        bool isSent;  // true = sent, false = received
    };
    QList<FrameEntry> frames;                   ///< Internal storage of captured frames
};

#endif // MESSAGEMODEL_H
