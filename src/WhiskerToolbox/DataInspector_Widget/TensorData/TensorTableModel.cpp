#include "TensorTableModel.hpp"

//https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots
#include "DataManager/Tensors/TensorData.hpp"
#define slots Q_SLOTS

#include <algorithm>
#include <span>
#include <stdexcept>

// =============================================================================
// Construction
// =============================================================================

TensorTableModel::TensorTableModel(QObject * parent)
    : QAbstractTableModel(parent) {}

// =============================================================================
// Data binding
// =============================================================================

void TensorTableModel::setTensorData(TensorData * tensor_data) {
    beginResetModel();
    _tensor_data = tensor_data;
    _resetDimensionMapping();
    endResetModel();
}

// =============================================================================
// Dimension mapping
// =============================================================================

void TensorTableModel::setRowDimension(int dim) {
    if (!_tensor_data) {
        return;
    }
    auto const nd = static_cast<int>(ndim());
    if (dim < 0 || dim >= nd || dim == _col_dim) {
        return;
    }
    beginResetModel();
    _row_dim = dim;
    endResetModel();
}

void TensorTableModel::setColumnDimension(int dim) {
    if (!_tensor_data) {
        return;
    }
    auto const nd = static_cast<int>(ndim());
    if (dim < -1 || dim >= nd || dim == _row_dim) {
        return;
    }
    beginResetModel();
    _col_dim = dim;
    endResetModel();
}

void TensorTableModel::setFixedIndex(int dim, std::size_t index) {
    if (!_tensor_data) {
        return;
    }
    auto const nd = static_cast<int>(ndim());
    if (dim < 0 || dim >= nd) {
        return;
    }
    auto const shape = tensorShape();
    if (index >= shape[static_cast<std::size_t>(dim)]) {
        return;
    }
    beginResetModel();
    _fixed_indices[static_cast<std::size_t>(dim)] = index;
    endResetModel();
}

// =============================================================================
// Queries
// =============================================================================

std::size_t TensorTableModel::fixedIndex(int dim) const {
    if (dim < 0 || static_cast<std::size_t>(dim) >= _fixed_indices.size()) {
        return 0;
    }
    return _fixed_indices[static_cast<std::size_t>(dim)];
}

std::size_t TensorTableModel::ndim() const noexcept {
    if (!_tensor_data) {
        return 0;
    }
    return _tensor_data->ndim();
}

std::vector<std::size_t> TensorTableModel::tensorShape() const {
    if (!_tensor_data) {
        return {};
    }
    return _tensor_data->shape();
}

std::vector<std::string> TensorTableModel::axisNames() const {
    if (!_tensor_data) {
        return {};
    }
    auto const & dims = _tensor_data->dimensions();
    std::vector<std::string> names;
    names.reserve(dims.ndim());
    for (std::size_t i = 0; i < dims.ndim(); ++i) {
        names.push_back(dims.axis(i).name);
    }
    return names;
}

// =============================================================================
// QAbstractTableModel overrides
// =============================================================================

int TensorTableModel::rowCount(QModelIndex const & parent) const {
    if (parent.isValid() || !_tensor_data || ndim() == 0) {
        return 0;
    }
    auto const shape = tensorShape();
    return static_cast<int>(shape[static_cast<std::size_t>(_row_dim)]);
}

int TensorTableModel::columnCount(QModelIndex const & parent) const {
    if (parent.isValid() || !_tensor_data || ndim() == 0) {
        return 0;
    }
    if (_col_dim < 0) {
        return 1; // single "Value" column for 1-D tensors
    }
    auto const shape = tensorShape();
    return static_cast<int>(shape[static_cast<std::size_t>(_col_dim)]);
}

QVariant TensorTableModel::data(QModelIndex const & index, int role) const {
    if (!index.isValid() || role != Qt::DisplayRole || !_tensor_data) {
        return {};
    }

    auto const nd = ndim();
    if (nd == 0) {
        return {};
    }

    // Build multi-dimensional index: start with fixed indices, then overwrite
    // the row and column dimension positions.
    std::vector<std::size_t> indices = _fixed_indices;

    indices[static_cast<std::size_t>(_row_dim)] = static_cast<std::size_t>(index.row());
    if (_col_dim >= 0) {
        indices[static_cast<std::size_t>(_col_dim)] = static_cast<std::size_t>(index.column());
    }

    try {
        float value = _tensor_data->at(std::span<std::size_t const>{indices});
        return QVariant::fromValue(static_cast<double>(value));
    } catch (std::exception const &) {
        return QString("ERR");
    }
}

QVariant TensorTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole || !_tensor_data) {
        return {};
    }

    auto const & dims = _tensor_data->dimensions();

    if (orientation == Qt::Horizontal) {
        // Column headers
        if (_col_dim < 0) {
            return QString("Value");
        }
        // If the column dimension has named columns and it's the last axis, use them
        if (dims.hasColumnNames() &&
            static_cast<std::size_t>(_col_dim) == dims.ndim() - 1 &&
            section < static_cast<int>(dims.columnNames().size())) {
            return QString::fromStdString(dims.columnNames()[static_cast<std::size_t>(section)]);
        }
        // Otherwise: axis_name[index]
        auto const & ax = dims.axis(static_cast<std::size_t>(_col_dim));
        return QString::fromStdString(ax.name) + "[" + QString::number(section) + "]";
    }

    // Row headers — show the row index (or row label from RowDescriptor if row dim is axis 0)
    if (orientation == Qt::Vertical) {
        if (_row_dim == 0) {
            // Use RowDescriptor labels when viewing the primary axis
            auto const & rows = _tensor_data->rows();
            auto label = rows.labelAt(static_cast<std::size_t>(section));
            if (auto const * ordinal = std::get_if<std::size_t>(&label)) {
                return QVariant::fromValue(static_cast<qulonglong>(*ordinal));
            }
            if (auto const * tfi = std::get_if<TimeFrameIndex>(&label)) {
                return QVariant::fromValue(tfi->getValue());
            }
            // Interval rows — show "start-end"
            if (auto const * interval = std::get_if<TimeFrameInterval>(&label)) {
                return QString::number(interval->start.getValue()) + "-" +
                       QString::number(interval->end.getValue());
            }
        }
        // For non-primary axes, just show the index
        return section;
    }

    return {};
}

// =============================================================================
// Private helpers
// =============================================================================

void TensorTableModel::_resetDimensionMapping() {
    if (!_tensor_data || _tensor_data->isEmpty()) {
        _row_dim = 0;
        _col_dim = -1;
        _fixed_indices.clear();
        return;
    }

    auto const nd = ndim();
    auto const shape = tensorShape();

    // Initialize all fixed indices to 0
    _fixed_indices.assign(nd, 0);

    if (nd == 1) {
        _row_dim = 0;
        _col_dim = -1; // single value column
    } else {
        // Default: first axis = rows, second axis = columns
        _row_dim = 0;
        _col_dim = 1;
    }
}
