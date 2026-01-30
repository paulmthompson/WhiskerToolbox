#ifndef POINT_TABLE_VIEW_HPP
#define POINT_TABLE_VIEW_HPP

/**
 * @file PointTableView.hpp
 * @brief Table view widget for PointData
 * 
 * PointTableView provides a table view for PointData objects in the Center zone.
 * It displays point data in a table format with columns for frame, coordinates,
 * and group information.
 * 
 * ## Features
 * - Point data table with frame, x, y coordinates, and group information
 * - Frame navigation via double-click
 * - Row selection support
 * 
 * ## Relationship with PointInspector
 * While PointInspector (Properties zone) contains controls like export options
 * and image size settings, PointTableView focuses solely on displaying the data
 * in tabular format.
 * 
 * @see BaseDataView for the base class
 * @see PointTableModel for the underlying data model
 */

#include "DataInspector_Widget/Inspectors/BaseDataView.hpp"
#include "Entity/EntityTypes.hpp"

#include <memory>
#include <string>
#include <vector>

class PointTableModel;
class GroupManager;

class QTableView;
class QVBoxLayout;

/**
 * @brief Table view widget for PointData
 * 
 * Displays point data in a table format with frame navigation support.
 */
class PointTableView : public BaseDataView {
    Q_OBJECT

public:
    /**
     * @brief Construct the point table view
     * @param data_manager Shared DataManager for data access
     * @param parent Parent widget
     */
    explicit PointTableView(
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent = nullptr);

    ~PointTableView() override;

    // =========================================================================
    // IDataView Interface
    // =========================================================================

    void setActiveKey(std::string const & key) override;
    void removeCallbacks() override;
    void updateView() override;

    [[nodiscard]] DM_DataType getDataType() const override { return DM_DataType::Points; }
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("Point Table"); }

    // =========================================================================
    // Additional Methods
    // =========================================================================

    /**
     * @brief Set the group manager for group filtering support
     * @param group_manager GroupManager instance (can be nullptr)
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

    /**
     * @brief Get the selected frame indices from the table
     * @return Vector of frame indices for selected rows
     */
    [[nodiscard]] std::vector<int64_t> getSelectedFrames() const;

    /**
     * @brief Get the selected EntityIds from the table
     * @return Vector of EntityIds for selected rows
     */
    [[nodiscard]] std::vector<EntityId> getSelectedEntityIds() const;

    /**
     * @brief Get the underlying table view
     * @return Pointer to the QTableView
     */
    [[nodiscard]] QTableView * tableView() const { return _table_view; }

signals:
    /**
     * @brief Emitted when user requests to move selected points to a target key
     * @param target_key The key to move points to
     */
    void movePointsRequested(std::string const & target_key);

    /**
     * @brief Emitted when user requests to copy selected points to a target key
     * @param target_key The key to copy points to
     */
    void copyPointsRequested(std::string const & target_key);

    /**
     * @brief Emitted when user requests to move selected points to a group
     * @param group_id The group ID to move points to
     */
    void movePointsToGroupRequested(int group_id);

    /**
     * @brief Emitted when user requests to remove selected points from their groups
     */
    void removePointsFromGroupRequested();

    /**
     * @brief Emitted when user requests to delete selected points
     */
    void deletePointsRequested();

private slots:
    void _handleTableViewDoubleClicked(QModelIndex const & index);
    void _onDataChanged();
    void _onGroupChanged();

private:
    void _setupUi();
    void _connectSignals();
    void _showContextMenu(QPoint const & position);
    void _populateGroupSubmenu(QMenu * menu, bool for_moving);

    QVBoxLayout * _layout{nullptr};
    QTableView * _table_view{nullptr};
    PointTableModel * _table_model{nullptr};
    GroupManager * _group_manager{nullptr};
    int _callback_id{-1};
};

#endif // POINT_TABLE_VIEW_HPP
