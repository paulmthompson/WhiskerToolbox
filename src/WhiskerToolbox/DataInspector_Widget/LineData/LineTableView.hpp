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

#include <memory>

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
    [[nodiscard]] QTableView * tableView() const { return _table_view; }

    /**
     * @brief Scroll to show the specified frame in the table
     * @param frame The frame to scroll to
     */
    void scrollToFrame(int64_t frame);

private slots:
    void _handleTableViewDoubleClicked(QModelIndex const & index);
    void _onDataChanged();

private:
    void _setupUi();
    void _connectSignals();

    QVBoxLayout * _layout{nullptr};
    QTableView * _table_view{nullptr};
    LineTableModel * _table_model{nullptr};
    GroupManager * _group_manager{nullptr};
    int _callback_id{-1};
};

#endif // LINE_TABLE_VIEW_HPP
