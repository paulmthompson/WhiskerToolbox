#include "TensorDataView.hpp"

#include "TensorTableModel.hpp"

#include "DataManager/DataManager.hpp"

//https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots
#include "DataManager/Tensors/TensorData.hpp"
#define slots Q_SLOTS

#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QSpinBox>
#include <QTableView>
#include <QVBoxLayout>

#include <iostream>

TensorDataView::TensorDataView(
    std::shared_ptr<DataManager> data_manager,
    QWidget * parent)
    : BaseDataView(std::move(data_manager), parent)
    , _table_model(new TensorTableModel(this)) {
    _setupUi();
    _connectSignals();
}

TensorDataView::~TensorDataView() {
    removeCallbacks();
}

void TensorDataView::setActiveKey(std::string const & key) {
    _active_key = key;

    auto tensor_data = dataManager()->getData<TensorData>(_active_key);
    if (tensor_data) {
        _table_model->setTensorData(tensor_data.get());
    } else {
        _table_model->setTensorData(nullptr);
    }
    _rebuildDimensionControls();
    _updateShapeLabel();
}

void TensorDataView::removeCallbacks() {
    // TensorData view doesn't register callbacks currently
}

void TensorDataView::updateView() {
    if (!_active_key.empty() && _table_model) {
        auto tensor_data = dataManager()->getData<TensorData>(_active_key);
        _table_model->setTensorData(tensor_data.get());
        _rebuildDimensionControls();
        _updateShapeLabel();
    }
}

// =============================================================================
// UI Setup
// =============================================================================

void TensorDataView::_setupUi() {
    _layout = new QVBoxLayout(this);
    _layout->setContentsMargins(4, 4, 4, 4);
    _layout->setSpacing(4);

    // --- Shape info label ---
    _shape_label = new QLabel(QStringLiteral("No tensor data"), this);
    _shape_label->setWordWrap(true);
    _layout->addWidget(_shape_label);

    // --- Row / Column dimension selectors ---
    _dim_controls_widget = new QWidget(this);
    _dim_combo_layout = new QHBoxLayout(_dim_controls_widget);
    _dim_combo_layout->setContentsMargins(0, 0, 0, 0);
    _dim_combo_layout->setSpacing(4);

    _dim_combo_layout->addWidget(new QLabel(QStringLiteral("Rows:"), _dim_controls_widget));
    _row_dim_combo = new QComboBox(_dim_controls_widget);
    _row_dim_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    _dim_combo_layout->addWidget(_row_dim_combo);

    _dim_combo_layout->addWidget(new QLabel(QStringLiteral("Cols:"), _dim_controls_widget));
    _col_dim_combo = new QComboBox(_dim_controls_widget);
    _col_dim_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    _dim_combo_layout->addWidget(_col_dim_combo);

    _layout->addWidget(_dim_controls_widget);

    // --- Fixed dimension slicers (populated dynamically) ---
    _fixed_dims_widget = new QWidget(this);
    _fixed_dims_layout = new QGridLayout(_fixed_dims_widget);
    _fixed_dims_layout->setContentsMargins(0, 0, 0, 0);
    _fixed_dims_layout->setSpacing(4);
    _layout->addWidget(_fixed_dims_widget);

    // --- Table view ---
    _table_view = new QTableView(this);
    _table_view->setModel(_table_model);
    _table_view->setSelectionBehavior(QAbstractItemView::SelectItems);
    _table_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _table_view->setAlternatingRowColors(true);
    _table_view->horizontalHeader()->setStretchLastSection(true);

    _layout->addWidget(_table_view, /*stretch=*/1);
}

void TensorDataView::_connectSignals() {
    connect(_table_view, &QTableView::doubleClicked,
            this, &TensorDataView::_handleTableViewDoubleClicked);
    connect(_row_dim_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TensorDataView::_onRowDimChanged);
    connect(_col_dim_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TensorDataView::_onColDimChanged);
}

// =============================================================================
// Dimension controls
// =============================================================================

void TensorDataView::_rebuildDimensionControls() {
    // Block signals while repopulating combos
    _row_dim_combo->blockSignals(true);
    _col_dim_combo->blockSignals(true);

    _row_dim_combo->clear();
    _col_dim_combo->clear();

    // Clear old fixed-dim spinboxes
    for (auto * sb : _fixed_spinboxes) {
        sb->deleteLater();
    }
    _fixed_spinboxes.clear();
    _fixed_spinbox_dims.clear();
    // Also clear labels in the fixed layout
    while (_fixed_dims_layout->count() > 0) {
        auto * item = _fixed_dims_layout->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    auto const nd = _table_model->ndim();
    if (nd == 0) {
        _dim_controls_widget->setVisible(false);
        _fixed_dims_widget->setVisible(false);
        _row_dim_combo->blockSignals(false);
        _col_dim_combo->blockSignals(false);
        return;
    }

    _dim_controls_widget->setVisible(true);

    auto const names = _table_model->axisNames();
    auto const shape = _table_model->tensorShape();

    // Populate combos with axis entries: "name (size)"
    for (std::size_t i = 0; i < nd; ++i) {
        QString label = QString::fromStdString(names[i]) +
                        " (" + QString::number(shape[i]) + ")";
        _row_dim_combo->addItem(label, QVariant::fromValue(static_cast<int>(i)));
        _col_dim_combo->addItem(label, QVariant::fromValue(static_cast<int>(i)));
    }
    // Add "None" option for column dimension (single value column, useful for 1-D)
    _col_dim_combo->addItem(QStringLiteral("None"), QVariant::fromValue(-1));

    // Set current selections to match the model
    _row_dim_combo->setCurrentIndex(_table_model->rowDimension());
    if (_table_model->columnDimension() < 0) {
        _col_dim_combo->setCurrentIndex(static_cast<int>(nd)); // the "None" entry
    } else {
        _col_dim_combo->setCurrentIndex(_table_model->columnDimension());
    }

    _row_dim_combo->blockSignals(false);
    _col_dim_combo->blockSignals(false);

    // Build fixed-dimension spinboxes for axes that are neither row nor col
    int row_dim = _table_model->rowDimension();
    int col_dim = _table_model->columnDimension();
    int grid_row = 0;

    for (std::size_t i = 0; i < nd; ++i) {
        auto dim_i = static_cast<int>(i);
        if (dim_i == row_dim || dim_i == col_dim) {
            continue;
        }
        auto * label = new QLabel(
            QString::fromStdString(names[i]) + " slice:", _fixed_dims_widget);
        auto * spinbox = new QSpinBox(_fixed_dims_widget);
        spinbox->setRange(0, static_cast<int>(shape[i]) - 1);
        spinbox->setValue(static_cast<int>(_table_model->fixedIndex(dim_i)));
        spinbox->setProperty("tensorDim", dim_i);

        connect(spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &TensorDataView::_onFixedIndexChanged);

        _fixed_dims_layout->addWidget(label, grid_row, 0);
        _fixed_dims_layout->addWidget(spinbox, grid_row, 1);
        _fixed_spinboxes.push_back(spinbox);
        _fixed_spinbox_dims.push_back(dim_i);
        ++grid_row;
    }

    _fixed_dims_widget->setVisible(grid_row > 0);
}

void TensorDataView::_updateShapeLabel() {
    auto const nd = _table_model->ndim();
    if (nd == 0) {
        _shape_label->setText(QStringLiteral("No tensor data"));
        return;
    }

    auto const shape = _table_model->tensorShape();
    auto const names = _table_model->axisNames();

    // e.g. "3D Tensor — time(100) × channel(50) × frequency(32)"
    QString text = QString::number(nd) + "D Tensor — ";
    for (std::size_t i = 0; i < nd; ++i) {
        if (i > 0) {
            text += QStringLiteral(" × ");
        }
        text += QString::fromStdString(names[i]) +
                "(" + QString::number(shape[i]) + ")";
    }
    _shape_label->setText(text);
}

// =============================================================================
// Slots
// =============================================================================

void TensorDataView::_onRowDimChanged(int combo_index) {
    if (combo_index < 0) {
        return;
    }
    int dim = _row_dim_combo->itemData(combo_index).toInt();
    if (dim == _table_model->columnDimension()) {
        // Swap: put the old row dim into col
        int old_row = _table_model->rowDimension();
        _table_model->setColumnDimension(old_row);
    }
    _table_model->setRowDimension(dim);
    _rebuildDimensionControls();
}

void TensorDataView::_onColDimChanged(int combo_index) {
    if (combo_index < 0) {
        return;
    }
    int dim = _col_dim_combo->itemData(combo_index).toInt();
    if (dim >= 0 && dim == _table_model->rowDimension()) {
        // Swap: put the old col dim into row
        int old_col = _table_model->columnDimension();
        if (old_col >= 0) {
            _table_model->setRowDimension(old_col);
        }
    }
    _table_model->setColumnDimension(dim);
    _rebuildDimensionControls();
}

void TensorDataView::_onFixedIndexChanged() {
    auto * spinbox = qobject_cast<QSpinBox *>(sender());
    if (!spinbox) {
        return;
    }
    int dim = spinbox->property("tensorDim").toInt();
    _table_model->setFixedIndex(dim, static_cast<std::size_t>(spinbox->value()));
}

void TensorDataView::_handleTableViewDoubleClicked(QModelIndex const & index) {
    if (!index.isValid()) {
        return;
    }

    // Only emit frameSelected if the row dimension is axis 0 (the primary time axis)
    if (_table_model->rowDimension() != 0) {
        return;
    }

    auto tensor_data = dataManager()->getData<TensorData>(_active_key);
    if (!tensor_data) {
        return;
    }

    auto tf = tensor_data->getTimeFrame();
    if (!tf) {
        std::cout << "TensorDataView::_handleTableViewDoubleClicked: TimeFrame not found for "
                  << _active_key << std::endl;
        return;
    }

    auto const & rows = tensor_data->rows();
    if (rows.type() == RowType::TimeFrameIndex) {
        auto label = rows.labelAt(static_cast<std::size_t>(index.row()));
        if (auto const * tfi = std::get_if<TimeFrameIndex>(&label)) {
            emit frameSelected(TimePosition(tfi->getValue(), tf));
        }
    }
}
