#ifndef TIMEFRAME_TABLE_MODEL_HPP
#define TIMEFRAME_TABLE_MODEL_HPP

/**
 * @file TimeFrameTableModel.hpp
 * @brief Table model for displaying TimeFrame values
 *
 * Displays each entry in a TimeFrame as a row with columns:
 * - Index: the TimeFrameIndex (0-based position)
 * - Time: the absolute time value stored at that index
 * - Group: the group name for the entity at that index (if any)
 *
 * Supports group filtering: when a group filter is set, only entries
 * whose EntityId belongs to that group are displayed.
 *
 * @see TimeFrame for the underlying data
 * @see TimeFrameDataView for the view widget that uses this model
 */

#include "Entity/EntityRegistry.hpp"
#include "Entity/EntityTypes.hpp"

#include <QAbstractTableModel>

#include <string>
#include <vector>

class GroupManager;

struct TimeFrameTableRow {
    int64_t index;       ///< TimeFrameIndex value
    int time_value;      ///< Absolute time at this index
    EntityId entity_id;  ///< EntityId for this time entry
    QString group_name;  ///< Name of the group this entry belongs to
};

class TimeFrameTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    /// Column indices
    enum Column {
        IndexColumn = 0,
        TimeColumn = 1,
        GroupColumn = 2,
        ColumnCount = 3
    };

    explicit TimeFrameTableModel(QObject * parent = nullptr)
        : QAbstractTableModel(parent) {}

    /**
     * @brief Set the time values and associated entity information
     * @param times Vector of time values (index in vector = TimeFrameIndex)
     * @param time_key The TimeFrame key (used for EntityRegistry lookups)
     * @param registry The EntityRegistry for resolving EntityIds
     */
    void setTimeValues(std::vector<int> const & times,
                       std::string const & time_key,
                       EntityRegistry * registry);

    void setGroupManager(GroupManager * group_manager);
    void setGroupFilter(int group_id);
    void clearGroupFilter();

    [[nodiscard]] int64_t getIndex(int row) const {
        if (row >= 0 && row < static_cast<int>(_display_data.size())) {
            return _display_data[static_cast<size_t>(row)].index;
        }
        return -1;
    }

    [[nodiscard]] int getTimeValue(int row) const {
        return _display_data.at(static_cast<size_t>(row)).time_value;
    }

    [[nodiscard]] int rowCount(QModelIndex const & parent = QModelIndex()) const override {
        Q_UNUSED(parent);
        return static_cast<int>(_display_data.size());
    }

    [[nodiscard]] int columnCount(QModelIndex const & parent = QModelIndex()) const override {
        Q_UNUSED(parent);
        return ColumnCount;
    }

    [[nodiscard]] QVariant data(QModelIndex const & index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || role != Qt::DisplayRole) {
            return QVariant{};
        }

        auto const row = static_cast<size_t>(index.row());
        if (row >= _display_data.size()) {
            return QVariant{};
        }

        auto const & row_data = _display_data[row];
        switch (index.column()) {
            case IndexColumn:
                return QVariant::fromValue(static_cast<qlonglong>(row_data.index));
            case TimeColumn:
                return QVariant::fromValue(row_data.time_value);
            case GroupColumn:
                return row_data.group_name;
            default:
                return QVariant{};
        }
    }

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
        if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
            return QVariant{};
        }

        switch (section) {
            case IndexColumn:
                return QStringLiteral("Index");
            case TimeColumn:
                return QStringLiteral("Time");
            case GroupColumn:
                return QStringLiteral("Group");
            default:
                return QVariant{};
        }
    }

private:
    void _applyGroupFilter();

    std::vector<TimeFrameTableRow> _all_data;
    std::vector<TimeFrameTableRow> _display_data;
    GroupManager * _group_manager{nullptr};
    int _filtered_group_id{-1};
};

#endif // TIMEFRAME_TABLE_MODEL_HPP
