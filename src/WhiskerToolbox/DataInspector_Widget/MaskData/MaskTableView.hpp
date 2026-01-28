#ifndef MASK_TABLE_VIEW_HPP
#define MASK_TABLE_VIEW_HPP

/**
 * @file MaskTableView.hpp
 * @brief Table view widget for MaskData
 * 
 * MaskTableView provides a table view for MaskData objects in the Center zone.
 * It displays mask data in a table format with columns for frame, mask index,
 * point count, and group information.
 * 
 * @see BaseDataView for the base class
 * @see MaskTableModel for the underlying data model
 */

#include "DataInspector_Widget/Inspectors/BaseDataView.hpp"

#include <memory>

class MaskTableModel;
class GroupManager;

class QTableView;
class QVBoxLayout;

/**
 * @brief Table view widget for MaskData
 */
class MaskTableView : public BaseDataView {
    Q_OBJECT

public:
    explicit MaskTableView(
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent = nullptr);

    ~MaskTableView() override;

    // IDataView Interface
    void setActiveKey(std::string const & key) override;
    void removeCallbacks() override;
    void updateView() override;

    [[nodiscard]] DM_DataType getDataType() const override { return DM_DataType::Mask; }
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("Mask Table"); }

    void setGroupManager(GroupManager * group_manager);
    void setGroupFilter(int group_id);
    void clearGroupFilter();

    [[nodiscard]] std::vector<int64_t> getSelectedFrames() const;
    [[nodiscard]] QTableView * tableView() const { return _table_view; }

private slots:
    void _handleTableViewDoubleClicked(QModelIndex const & index);
    void _onDataChanged();

private:
    void _setupUi();
    void _connectSignals();

    QVBoxLayout * _layout{nullptr};
    QTableView * _table_view{nullptr};
    MaskTableModel * _table_model{nullptr};
    GroupManager * _group_manager{nullptr};
    int _callback_id{-1};
};

#endif // MASK_TABLE_VIEW_HPP
