#ifndef INSPECTOR_TABLE_SORT_HPP
#define INSPECTOR_TABLE_SORT_HPP

/**
 * @file InspectorTableSort.hpp
 * @brief Shared stable-sort helper for Data Inspector table models
 */

#include <Qt>

#include <QHeaderView>
#include <QTableView>

#include <algorithm>
#include <vector>

/**
 * @brief Stable-sort display rows by column using a model-specific comparator.
 *
 * @tparam RowT Row struct stored in the model's _display_data vector
 * @tparam ColumnLessFn Callable with signature bool(int column, RowT const&, RowT const&)
 *                      returning true when the first row is less than the second for ascending order.
 * @param rows Mutable display data to sort in place
 * @param column Column index to sort by
 * @param order Ascending or descending sort order
 * @param columnLess Model-specific column comparator
 */
/**
 * @brief Enable header sorting with default ascending order on column 0 (Frame / Start).
 *
 * QTableView re-applies the header sort indicator on every model reset. Without an
 * explicit indicator, Qt may sort descending and fight the model's default sort.
 *
 * @param table_view Table view to configure (must already have a model set)
 */
inline void configureDefaultInspectorTableSort(QTableView * table_view) {
    if (!table_view) {
        return;
    }

    QHeaderView * const header = table_view->horizontalHeader();
    header->setSortIndicator(0, Qt::AscendingOrder);
    table_view->setSortingEnabled(true);
    table_view->sortByColumn(0, Qt::AscendingOrder);
}

template<typename RowT, typename ColumnLessFn>
void stableSortDisplayRows(
        std::vector<RowT> & rows,
        int column,
        Qt::SortOrder order,
        ColumnLessFn columnLess) {
    if (rows.empty()) {
        return;
    }

    auto const compareAscending = [&](RowT const & lhs, RowT const & rhs) {
        return columnLess(column, lhs, rhs);
    };

    if (order == Qt::AscendingOrder) {
        std::stable_sort(rows.begin(), rows.end(), compareAscending);
    } else {
        std::stable_sort(rows.begin(), rows.end(), [&](RowT const & lhs, RowT const & rhs) {
            return compareAscending(rhs, lhs);
        });
    }
}

#endif// INSPECTOR_TABLE_SORT_HPP
