#ifndef TENSOR_DATA_VIEW_HPP
#define TENSOR_DATA_VIEW_HPP

/**
 * @file TensorDataView.hpp
 * @brief Table view widget for TensorData with dimension controls
 *
 * TensorDataView provides a table view for the refactored TensorData objects.
 * It displays:
 *   - A summary label showing tensor shape and dimension info
 *   - ComboBoxes for selecting which dimension maps to rows/columns
 *   - SpinBoxes for choosing the slice index of all other ("fixed") dimensions
 *   - A QTableView that lazily populates cell values as the user scrolls
 *
 * @see BaseDataView for the base class
 * @see TensorTableModel for the underlying data model
 */

#include "DataInspector_Widget/Inspectors/BaseDataView.hpp"

#include <memory>
#include <vector>

class TensorTableModel;

class QComboBox;
class QLabel;
class QSpinBox;
class QTableView;
class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QWidget;

/**
 * @brief Table view widget for TensorData with interactive dimension controls
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
    void _onRowDimChanged(int combo_index);
    void _onColDimChanged(int combo_index);
    void _onFixedIndexChanged();

private:
    void _setupUi();
    void _connectSignals();
    void _rebuildDimensionControls();
    void _updateShapeLabel();

    // Layout
    QVBoxLayout * _layout{nullptr};

    // Info section
    QLabel * _shape_label{nullptr};

    // Dimension mapping controls
    QWidget * _dim_controls_widget{nullptr};
    QHBoxLayout * _dim_combo_layout{nullptr};
    QComboBox * _row_dim_combo{nullptr};
    QComboBox * _col_dim_combo{nullptr};

    // Fixed dimension slicers
    QWidget * _fixed_dims_widget{nullptr};
    QGridLayout * _fixed_dims_layout{nullptr};
    std::vector<QSpinBox *> _fixed_spinboxes;   ///< One per "other" dimension
    std::vector<int> _fixed_spinbox_dims;        ///< Maps spinbox index → tensor axis index

    // Table
    QTableView * _table_view{nullptr};
    TensorTableModel * _table_model{nullptr};
};

#endif // TENSOR_DATA_VIEW_HPP
