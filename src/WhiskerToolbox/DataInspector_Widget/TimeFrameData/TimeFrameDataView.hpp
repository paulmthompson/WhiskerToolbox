#ifndef TIMEFRAME_DATA_VIEW_HPP
#define TIMEFRAME_DATA_VIEW_HPP

/**
 * @file TimeFrameDataView.hpp
 * @brief Table view widget for TimeFrame objects
 * 
 * TimeFrameDataView provides a table view of TimeFrame data in the Center zone.
 * It displays each entry with its index and corresponding time value.
 * 
 * @see BaseDataView for the base class
 * @see TimeFrameTableModel for the underlying data model
 * @see TimeFrameInspector for the properties panel
 */

#include "DataInspector_Widget/Inspectors/BaseDataView.hpp"

class GroupManager;
class TimeFrameTableModel;

class QLabel;
class QTableView;
class QVBoxLayout;

/**
 * @brief Table view widget for TimeFrame objects
 * 
 * Displays TimeFrame data as a table with Index and Time columns.
 * Unlike data views, this works with TimeFrame keys rather than data keys.
 */
class TimeFrameDataView : public BaseDataView {
    Q_OBJECT

public:
    explicit TimeFrameDataView(
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent = nullptr);

    ~TimeFrameDataView() override;

    // IDataView Interface
    void setActiveKey(std::string const & key) override;
    void removeCallbacks() override;
    void updateView() override;

    [[nodiscard]] DM_DataType getDataType() const override { return DM_DataType::Time; }
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("TimeFrame Table"); }

    [[nodiscard]] QTableView * tableView() const { return _table_view; }

    void setGroupManager(GroupManager * group_manager);
    void setGroupFilter(int group_id);
    void clearGroupFilter();

private slots:
    void _onGroupChanged();

private:
    void _setupUi();

    QVBoxLayout * _layout{nullptr};
    QTableView * _table_view{nullptr};
    TimeFrameTableModel * _table_model{nullptr};
    QLabel * _info_label{nullptr};
    GroupManager * _group_manager{nullptr};
};

#endif // TIMEFRAME_DATA_VIEW_HPP
