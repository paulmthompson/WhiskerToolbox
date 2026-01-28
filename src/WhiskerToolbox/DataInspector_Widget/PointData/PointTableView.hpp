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

#include <memory>

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
     * @brief Get the underlying table view
     * @return Pointer to the QTableView
     */
    [[nodiscard]] QTableView * tableView() const { return _table_view; }

private slots:
    void _handleTableViewDoubleClicked(QModelIndex const & index);
    void _onDataChanged();

private:
    void _setupUi();
    void _connectSignals();

    QVBoxLayout * _layout{nullptr};
    QTableView * _table_view{nullptr};
    PointTableModel * _table_model{nullptr};
    GroupManager * _group_manager{nullptr};
    int _callback_id{-1};
};

#endif // POINT_TABLE_VIEW_HPP
