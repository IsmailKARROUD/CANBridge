/**
 * @file framefilterproxymodel.h
 * @brief Definition of FrameFilterProxyModel for filtering the Analyzer table.
 *
 * Sits between MessageModel and QTableView, filtering rows by:
 *  - Direction (TX / RX / All)
 *  - CAN ID (hex substring match)
 *  - Data payload (hex substring match)
 */

#ifndef FRAMEFILTERPROXYMODEL_H
#define FRAMEFILTERPROXYMODEL_H

#include <QSortFilterProxyModel>

/**
 * @class FrameFilterProxyModel
 * @brief Proxy model that filters CAN frame rows by direction, ID, and data.
 *
 * Filter criteria are combined with AND logic: a row must match all
 * active filters to be displayed.
 */
class FrameFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit FrameFilterProxyModel(QObject *parent = nullptr);

    /// Set the direction filter: "All", "TX", or "RX".
    void setDirectionFilter(const QString& direction);

    /// Set the CAN ID filter (case-insensitive hex substring match).
    void setIdFilter(const QString& idFilter);

    /// Set the data payload filter (case-insensitive hex substring match).
    void setDataFilter(const QString& dataFilter);

protected:
    /// Returns true if the row matches all active filters.
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_directionFilter = "All";  ///< Current direction filter
    QString m_idFilter;                 ///< Current ID filter text
    QString m_dataFilter;               ///< Current data filter text
};

#endif // FRAMEFILTERPROXYMODEL_H
