#ifndef TENSOR_DATA_VIEW_HPP
#define TENSOR_DATA_VIEW_HPP

/**
 * @file TensorDataView.hpp
 * @brief Table view widget for TensorData
 * 
 * TensorDataView provides a table view for TensorData objects in the Center zone.
 * It displays tensor data in a table format with columns for frame and shape
 * information.
 * 
 * @see BaseDataView for the base class
 * @see TensorTableModel for the underlying data model
 */

#include "DataInspector_Widget/Inspectors/BaseDataView.hpp"

#include <memory>

class TensorTableModel;

class QTableView;
class QVBoxLayout;

/**
 * @brief Table view widget for TensorData
 */
class TensorDataView : public BaseDataView {
    Q_OBJECT

public:
    explicit TensorDataView(
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent = nullptr);

    ~TensorDataView() override;

    // IDataView Interface
    void setActiveKey(std::string const & key) override;
    void removeCallbacks() override;
    void updateView() override;

    [[nodiscard]] DM_DataType getDataType() const override { return DM_DataType::Tensor; }
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("Tensor Table"); }

    [[nodiscard]] QTableView * tableView() const { return _table_view; }

private slots:
    void _handleTableViewDoubleClicked(QModelIndex const & index);

private:
    void _setupUi();
    void _connectSignals();

    QVBoxLayout * _layout{nullptr};
    QTableView * _table_view{nullptr};
    TensorTableModel * _table_model{nullptr};
};

#endif // TENSOR_DATA_VIEW_HPP
