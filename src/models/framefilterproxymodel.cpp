/**
 * @file framefilterproxymodel.cpp
 * @brief Implementation of FrameFilterProxyModel.
 *
 * Filters Analyzer table rows by direction (TX/RX), CAN ID substring,
 * and data payload substring. All filters use AND logic.
 *
 * This proxy model sits between MessageModel (source) and QTableView (view):
 *   QTableView  ->  FrameFilterProxyModel  ->  MessageModel
 * The proxy intercepts row access and hides rows that don't match the
 * active filter criteria, without modifying the underlying data.
 */

#include "framefilterproxymodel.h"

// ============================================================================
// Constructor
// ============================================================================

FrameFilterProxyModel::FrameFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

// ============================================================================
// Filter Setters
// Each setter stores the new filter value and uses beginResetModel()/endResetModel()
// to force Qt to re-evaluate filterAcceptsRow() for every row immediately.
// ============================================================================

/**
 * Set the direction filter.
 * "All" disables direction filtering; "TX" or "RX" shows only matching rows.
 */
void FrameFilterProxyModel::setDirectionFilter(const QString& direction)
{
    beginResetModel();
    m_directionFilter = direction;
    endResetModel();
}

/**
 * Set the CAN ID filter text.
 * An empty string disables ID filtering. Any non-empty string filters rows
 * where column 2 (ID) contains the text as a substring (case-insensitive).
 * For example, typing "1A" matches both "0x1A3" and "0x01A".
 */
void FrameFilterProxyModel::setIdFilter(const QString& idFilter)
{
    beginResetModel();
    m_idFilter = idFilter;
    endResetModel();
}

/**
 * Set the data payload filter text.
 * Works like the ID filter but matches against column 4 (data bytes).
 * For example, typing "FF 01" matches any frame whose data contains "FF 01".
 */
void FrameFilterProxyModel::setDataFilter(const QString& dataFilter)
{
    beginResetModel();
    m_dataFilter = dataFilter;
    endResetModel();
}

// ============================================================================
// Core Filter Logic
// ============================================================================

/**
 * Called by Qt for each row in the source model to decide visibility.
 *
 * A row passes the filter only if it matches ALL active criteria (AND logic):
 *  - Direction: column 1 must exactly equal the filter (unless filter is "All")
 *  - ID:        column 2 text must contain the filter as a substring
 *  - Data:      column 4 text must contain the filter as a substring
 *
 * Column indices correspond to the MessageModel layout:
 *   0=Timestamp, 1=Dir, 2=ID, 3=DLC, 4=Data
 *
 * @param sourceRow     Row index in the source MessageModel
 * @param sourceParent  Parent index (unused — flat table model)
 * @return true if the row should be visible, false to hide it
 */
bool FrameFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    // Check direction filter against column 1 ("TX" or "RX")
    if (m_directionFilter != "All") {
        QModelIndex dirIndex = sourceModel()->index(sourceRow, 1, sourceParent);
        QString dir = sourceModel()->data(dirIndex).toString();
        if (dir != m_directionFilter) {
            return false;
        }
    }

    // Check ID filter against column 2 (e.g., "0x123") — substring match
    if (!m_idFilter.isEmpty()) {
        QModelIndex idIndex = sourceModel()->index(sourceRow, 2, sourceParent);
        QString id = sourceModel()->data(idIndex).toString();
        if (!id.contains(m_idFilter, Qt::CaseInsensitive)) {
            return false;
        }
    }

    // Check data filter against column 4 (e.g., "01 FF A0") — substring match
    if (!m_dataFilter.isEmpty()) {
        if (!m_dataFilter.isEmpty()) {
            QModelIndex dataIndex = sourceModel()->index(sourceRow, 4, sourceParent);
            QString data = sourceModel()->data(dataIndex).toString();

            // Remove spaces from both the data and filter for flexible matching
            QString normalizedData = data.remove(' ');
            QString normalizedFilter = m_dataFilter.trimmed().remove(' ');

            if (!normalizedData.contains(normalizedFilter, Qt::CaseInsensitive)) {
                return false;
            }
        }
    }

    // Row matches all active filters — show it
    return true;
}
