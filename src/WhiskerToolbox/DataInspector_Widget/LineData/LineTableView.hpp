#ifndef LINE_TABLE_VIEW_HPP
#define LINE_TABLE_VIEW_HPP

/**
 * @file LineTableView.hpp
 * @brief Table view widget for LineData
 * 
 * LineTableView provides a table view for LineData objects in the Center zone.
 * It displays line data in a table format with columns for frame, line index,
 * length, and group information.
 * 
 * @see BaseDataView for the base class
 * @see LineTableModel for the underlying data model
 */

#include "DataInspector_Widget/Inspectors/BaseDataView.hpp"

#include "Entity/EntityTypes.hpp"

#include <memory>
#include <string>
#include <vector>

class LineTableModel;
class GroupManager;

class QTableView;
class QVBoxLayout;

/**
 * @brief Table view widget for LineData
 */
class LineTableView : public BaseDataView {
    Q_OBJECT

public:
    explicit LineTableView(
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent = nullptr);

    ~LineTableView() override;

    // IDataView Interface
    void setActiveKey(std::string const & key) override;
    void removeCallbacks() override;
    void updateView() override;

    [[nodiscard]] DM_DataType getDataType() const override { return DM_DataType::Line; }
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("Line Table"); }

    void setGroupManager(GroupManager * group_manager);
    void setGroupFilter(int group_id);
    void clearGroupFilter();

    [[nodiscard]] std::vector<int64_t> getSelectedFrames() const;
    [[nodiscard]] std::vector<EntityId> getSelectedEntityIds() const;
    [[nodiscard]] QTableView * tableView() const { return _table_view; }

    /**
     * @brief Scroll to show the specified frame in the table
     * @param frame The frame to scroll to
     */
    void scrollToFrame(int64_t frame);

private slots:
    void _handleTableViewDoubleClicked(QModelIndex const & index);
    void _onDataChanged();
    void _showContextMenu(QPoint const & position);
    void _onGroupChanged();

signals:
    /**
     * @brief Emitted when user requests to move selected lines to a target key
     * @param target_key The key to move lines to
     */
    void moveLinesRequested(std::string const & target_key);

    /**
     * @brief Emitted when user requests to copy selected lines to a target key
     * @param target_key The key to copy lines to
     */
    void copyLinesRequested(std::string const & target_key);

    /**
     * @brief Emitted when user requests to delete selected lines
     */
    void deleteLinesRequested();

    /**
     * @brief Emitted when user requests to move selected lines to a group
     * @param group_id The group ID to move lines to
     */
    void moveLinesToGroupRequested(int group_id);

    /**
     * @brief Emitted when user requests to remove selected lines from their groups
     */
    void removeLinesFromGroupRequested();

private:
    void _setupUi();
    void _connectSignals();
    void _populateGroupSubmenu(QMenu * menu, bool for_moving);

    QVBoxLayout * _layout{nullptr};
    QTableView * _table_view{nullptr};
    LineTableModel * _table_model{nullptr};
    GroupManager * _group_manager{nullptr};
    int _callback_id{-1};
};

#endif // LINE_TABLE_VIEW_HPP
