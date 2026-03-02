#ifndef DIGITAL_INTERVAL_SERIES_DATA_VIEW_HPP
#define DIGITAL_INTERVAL_SERIES_DATA_VIEW_HPP

/**
 * @file DigitalIntervalSeriesDataView.hpp
 * @brief Table view widget for DigitalIntervalSeries data
 * 
 * DigitalIntervalSeriesDataView provides a table view for DigitalIntervalSeries
 * objects in the Center zone. It displays intervals in a table format with
 * start and end frame/time information, plus group column.
 * 
 * ## Features
 * - Group filtering via GroupManager
 * - Group ID column in table
 * - Right-click context menu for move/copy/delete and group management
 * 
 * @see BaseDataView for the base class
 * @see IntervalTableModel for the underlying data model
 */

#include "DataInspector_Widget/Inspectors/BaseDataView.hpp"

#include "Entity/EntityTypes.hpp"
#include "TimeFrame/interval_data.hpp"

#include <memory>
#include <vector>

class IntervalTableModel;
class GroupManager;

class QTableView;
class QVBoxLayout;
class QMenu;

/**
 * @brief Table view widget for DigitalIntervalSeries
 */
class DigitalIntervalSeriesDataView : public BaseDataView {
    Q_OBJECT

public:
    explicit DigitalIntervalSeriesDataView(
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent = nullptr);

    ~DigitalIntervalSeriesDataView() override;

    // IDataView Interface
    void setActiveKey(std::string const & key) override;
    void removeCallbacks() override;
    void updateView() override;

    [[nodiscard]] DM_DataType getDataType() const override { return DM_DataType::DigitalInterval; }
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("Interval Table"); }

    [[nodiscard]] QTableView * tableView() const { return _table_view; }

    /**
     * @brief Get the currently selected intervals from the table view
     * @return Vector of selected intervals
     */
    [[nodiscard]] std::vector<Interval> getSelectedIntervals() const;

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
     * @brief Emitted when user requests to move selected intervals to a target key
     */
    void moveIntervalsRequested(std::string const & target_key);

    /**
     * @brief Emitted when user requests to copy selected intervals to a target key
     */
    void copyIntervalsRequested(std::string const & target_key);

    /**
     * @brief Emitted when user requests to delete selected intervals
     */
    void deleteIntervalsRequested();

    /**
     * @brief Emitted when user requests to move selected intervals to a group
     */
    void moveIntervalsToGroupRequested(int group_id);

    /**
     * @brief Emitted when user requests to remove selected intervals from their groups
     */
    void removeIntervalsFromGroupRequested();

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
    IntervalTableModel * _table_model{nullptr};
    GroupManager * _group_manager{nullptr};
    int _callback_id{-1};
};

#endif // DIGITAL_INTERVAL_SERIES_DATA_VIEW_HPP
