#ifndef DIGITAL_EVENT_SERIES_DATA_VIEW_HPP
#define DIGITAL_EVENT_SERIES_DATA_VIEW_HPP

/**
 * @file DigitalEventSeriesDataView.hpp
 * @brief Table view widget for DigitalEventSeries data
 * 
 * DigitalEventSeriesDataView provides a table view for DigitalEventSeries
 * objects in the Center zone. It displays events in a table format with
 * frame/time information, plus group column.
 * 
 * ## Features
 * - Group filtering via GroupManager
 * - Group ID column in table
 * - Right-click context menu for move/copy/delete and group management
 * 
 * @see BaseDataView for the base class
 * @see EventTableModel for the underlying data model
 */

#include "DataInspector_Widget/Inspectors/BaseDataView.hpp"

#include "Entity/EntityTypes.hpp"

#include <memory>
#include <vector>

class EventTableModel;
class GroupManager;

class QTableView;
class QVBoxLayout;
class QMenu;

/**
 * @brief Table view widget for DigitalEventSeries
 */
class DigitalEventSeriesDataView : public BaseDataView {
    Q_OBJECT

public:
    explicit DigitalEventSeriesDataView(
            std::shared_ptr<DataManager> data_manager,
            QWidget * parent = nullptr);

    ~DigitalEventSeriesDataView() override;

    // IDataView Interface
    void setActiveKey(std::string const & key) override;
    void removeCallbacks() override;
    void updateView() override;

    [[nodiscard]] DM_DataType getDataType() const override { return DM_DataType::DigitalEvent; }
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("Event Table"); }

    [[nodiscard]] QTableView * tableView() const { return _table_view; }

    /**
     * @brief Get the selected EntityIds from the table
     * @return Vector of EntityIds for selected rows
     */
    [[nodiscard]] std::vector<EntityId> getSelectedEntityIds() const;

    /**
     * @brief Set the GroupManager for group features
     * @param group_manager Pointer to GroupManager (can be nullptr)
     */
    void setGroupManager(GroupManager * group_manager);

    /**
     * @brief Apply a group filter to the table
     * @param group_id Group ID to filter by, or -1 to show all groups
     */
    void setGroupFilter(int group_id);

    /**
     * @brief Clear the group filter
     */
    void clearGroupFilter();

signals:
    /**
     * @brief Emitted when user requests to move selected events to a target key
     */
    void moveEventsRequested(std::string const & target_key);

    /**
     * @brief Emitted when user requests to copy selected events to a target key
     */
    void copyEventsRequested(std::string const & target_key);

    /**
     * @brief Emitted when user requests to delete selected events
     */
    void deleteEventsRequested();

    /**
     * @brief Emitted when user requests to move selected events to a group
     */
    void moveEventsToGroupRequested(int group_id);

    /**
     * @brief Emitted when user requests to remove selected events from their groups
     */
    void removeEventsFromGroupRequested();

private slots:
    void _handleTableViewDoubleClicked(QModelIndex const & index);
    void _onDataChanged();
    void _showContextMenu(QPoint const & position);
    void _onGroupChanged();

private:
    void _setupUi();
    void _connectSignals();
    void _populateGroupSubmenu(QMenu * menu, bool for_moving);

    QVBoxLayout * _layout{nullptr};
    QTableView * _table_view{nullptr};
    EventTableModel * _table_model{nullptr};
    GroupManager * _group_manager{nullptr};
    int _callback_id{-1};
};

#endif// DIGITAL_EVENT_SERIES_DATA_VIEW_HPP
