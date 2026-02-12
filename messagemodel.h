#ifndef MESSAGEMODEL_H
#define MESSAGEMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include "canframe.h"

class MessageModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit MessageModel(QObject *parent = nullptr);

    // Required QAbstractTableModel overrides
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

public slots:
    void addFrame(const CANFrame& frame);
    void clear();

signals:
    void frameAdded();

private:
    QList<CANFrame> frames;
    static const int MAX_FRAMES = 10000;  // Limit to prevent memory issues
};

#endif // MESSAGEMODEL_H
