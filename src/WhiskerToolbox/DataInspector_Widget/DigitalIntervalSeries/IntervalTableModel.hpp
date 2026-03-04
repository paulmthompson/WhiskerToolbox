#ifndef WHISKERTOOLBOX_INTERVALTABLEMODEL_HPP
#define WHISKERTOOLBOX_INTERVALTABLEMODEL_HPP

/**
 * @file IntervalTableModel.hpp
 * @brief Table model for DigitalIntervalSeries data with group support
 *
 * Provides a QAbstractTableModel for displaying interval data with columns
 * for Start, End, and Group name. Supports group filtering via GroupManager.
 */

#include "Entity/EntityTypes.hpp"
#include "TimeFrame/interval_data.hpp"

#include <QAbstractTableModel>
#include <QString>

#include <cstdint>
#include <vector>

class DigitalIntervalSeries;
class GroupManager;

/**
 * @brief Row data for the interval table model
 */
struct IntervalTableRow {
    Interval interval;
    EntityId entity_id;
    QString group_name;
};

class IntervalTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit IntervalTableModel(QObject * parent = nullptr);

    /**
     * @brief Populate the model from a DigitalIntervalSeries
     * @param interval_data Pointer to the series (can be nullptr to clear)
     */
    void setIntervals(DigitalIntervalSeries const * interval_data);

    /**
     * @brief Set the GroupManager for group name resolution
     * @param group_manager Pointer to GroupManager (can be nullptr)
     */
    void setGroupManager(GroupManager * group_manager);

    /**
     * @brief Set a group filter
     * @param group_id Group ID to filter by, or -1 to show all
     */
    void setGroupFilter(int group_id);

    /**
     * @brief Clear the group filter (show all groups)
     */
    void clearGroupFilter();

    [[nodiscard]] int rowCount(QModelIndex const & parent) const override;
    [[nodiscard]] int columnCount(QModelIndex const & parent) const override;
    [[nodiscard]] QVariant data(QModelIndex const & index, int role) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] Qt::ItemFlags flags(QModelIndex const & index) const override;

    /**
     * @brief Get the row data at a specific row
     * @param row Row index
     * @return IntervalTableRow data
     */
    [[nodiscard]] IntervalTableRow getRowData(int row) const;

    /**
     * @brief Get the interval at a specific row (convenience)
     * @param row Row index
     * @return Interval data
     */
    [[nodiscard]] Interval getInterval(int row) const;

private:
    std::vector<IntervalTableRow> _display_data;
    std::vector<IntervalTableRow> _all_data;
    DigitalIntervalSeries const * _interval_data_source{nullptr};
    GroupManager * _group_manager{nullptr};
    int _filtered_group_id{-1};

    void _applyGroupFilter();
};

#endif//WHISKERTOOLBOX_INTERVALTABLEMODEL_HPP
