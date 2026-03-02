#ifndef EVENTTABLEMODEL_HPP
#define EVENTTABLEMODEL_HPP

/**
 * @file EventTableModel.hpp
 * @brief Table model for DigitalEventSeries data with group support
 *
 * Provides a QAbstractTableModel for displaying event data with columns
 * for Frame and Group name. Supports group filtering via GroupManager.
 */

#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QAbstractTableModel>
#include <QString>

#include <vector>

class DigitalEventSeries;
class GroupManager;

/**
 * @brief Row data for the event table model
 */
struct EventTableRow {
    TimeFrameIndex time;
    EntityId entity_id;
    QString group_name;
};

class EventTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit EventTableModel(QObject * parent = nullptr);

    /**
     * @brief Populate the model from a DigitalEventSeries
     * @param event_data Pointer to the series (can be nullptr to clear)
     */
    void setEvents(DigitalEventSeries const * event_data);

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
     * @return EventTableRow data
     */
    [[nodiscard]] EventTableRow getRowData(int row) const;

    /**
     * @brief Get the event time at a specific row (convenience)
     * @param row Row index
     * @return TimeFrameIndex for the event
     */
    [[nodiscard]] TimeFrameIndex getEvent(int row) const;

private:
    std::vector<EventTableRow> _display_data;
    std::vector<EventTableRow> _all_data;
    DigitalEventSeries const * _event_data_source{nullptr};
    GroupManager * _group_manager{nullptr};
    int _filtered_group_id{-1};

    void _applyGroupFilter();
};

#endif// EVENTTABLEMODEL_HPP
