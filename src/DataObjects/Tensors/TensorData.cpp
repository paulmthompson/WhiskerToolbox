/**
 * @file TensorData.cpp
 * @brief Implementation of the refactored TensorData public API
 *
 * Wires together DimensionDescriptor, RowDescriptor, and TensorStorageWrapper
 * to provide a unified N-dimensional tensor type for DataManager.
 */

#include "Tensors/TensorData.hpp"

#include "Tensors/DimensionDescriptor.hpp"
#include "Tensors/RowDescriptor.hpp"
#include "Tensors/storage/ArmadilloTensorStorage.hpp"
#include "Tensors/storage/DenseTensorStorage.hpp"
#include "Tensors/storage/LazyColumnTensorStorage.hpp"
#include "Tensors/storage/TensorStorageWrapper.hpp"

#ifdef TENSOR_BACKEND_LIBTORCH
#include "Tensors/storage/LibTorchTensorStorage.hpp"
#endif

#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

// =============================================================================
// Default / Special Members
// =============================================================================

TensorData::TensorData()
    : _dimensions{},
      _rows{RowDescriptor::ordinal(0)},
      _storage{},
      _time_frame{nullptr} {
}

TensorData::~TensorData() = default;

TensorData::TensorData(TensorData const & other) = default;
TensorData & TensorData::operator=(TensorData const & other) = default;

TensorData::TensorData(TensorData && other) noexcept = default;
TensorData & TensorData::operator=(TensorData && other) noexcept = default;

// =============================================================================
// Private Constructor
// =============================================================================

TensorData::TensorData(DimensionDescriptor dimensions,
                       RowDescriptor rows,
                       TensorStorageWrapper storage,
                       std::shared_ptr<TimeFrame> time_frame)
    : _dimensions{std::move(dimensions)},
      _rows{std::move(rows)},
      _storage{std::move(storage)},
      _time_frame{std::move(time_frame)} {
}

// =============================================================================
// Helper: build storage from flat row-major data + shape
// =============================================================================

namespace {

/**
 * @brief Create the appropriate storage backend based on dimensionality
 *
 * Armadillo for ≤3D (zero-copy mlpack interop), Dense for >3D.
 */
TensorStorageWrapper makeStorage(std::vector<float> const & data,
                                 std::vector<std::size_t> const & shape) {
    auto const ndim = shape.size();

    if (ndim == 0) {
        throw std::invalid_argument(
                "TensorData: cannot create storage with zero dimensions");
    }

    // Validate total element count
    std::size_t total = 1;
    for (auto s: shape) {
        total *= s;
    }
    if (data.size() != total) {
        throw std::invalid_argument(
                "TensorData: data size (" + std::to_string(data.size()) +
                ") doesn't match shape product (" + std::to_string(total) + ")");
    }

    if (ndim == 1) {
        // 1D → arma::fvec
        arma::fvec v(data.size());
        std::copy(data.begin(), data.end(), v.begin());
        return TensorStorageWrapper{ArmadilloTensorStorage{std::move(v)}};
    }

    if (ndim == 2) {
        // 2D → ArmadilloTensorStorage from flat row-major data
        return TensorStorageWrapper{
                ArmadilloTensorStorage{data, shape[0], shape[1]}};
    }

    if (ndim == 3) {
        // 3D → arma::fcube
        // ArmadilloTensorStorage interprets cube shape as [n_slices, n_rows, n_cols]
        // Input data is row-major: iterate slices, then rows, then cols
        auto const n_slices = shape[0];
        auto const n_rows = shape[1];
        auto const n_cols = shape[2];
        arma::fcube cube(n_rows, n_cols, n_slices);

        for (std::size_t s = 0; s < n_slices; ++s) {
            for (std::size_t r = 0; r < n_rows; ++r) {
                for (std::size_t c = 0; c < n_cols; ++c) {
                    auto flat_idx = s * (n_rows * n_cols) + r * n_cols + c;
                    cube(r, c, s) = data[flat_idx];
                }
            }
        }
        return TensorStorageWrapper{ArmadilloTensorStorage{std::move(cube)}};
    }

    // >3D → DenseTensorStorage (row-major flat vector)
    return TensorStorageWrapper{DenseTensorStorage{data, shape}};
}

}// anonymous namespace

// =============================================================================
// Factory: createTimeSeries2D
// =============================================================================

TensorData TensorData::createTimeSeries2D(
        std::vector<float> const & data,
        std::size_t num_rows,
        std::size_t num_cols,
        std::shared_ptr<TimeIndexStorage> time_storage,
        std::shared_ptr<TimeFrame> time_frame,
        std::vector<std::string> column_names) {
    if (!time_storage) {
        throw std::invalid_argument(
                "TensorData::createTimeSeries2D: time_storage must not be null");
    }
    if (time_storage->size() != num_rows) {
        throw std::invalid_argument(
                "TensorData::createTimeSeries2D: time_storage size (" +
                std::to_string(time_storage->size()) +
                ") must match num_rows (" + std::to_string(num_rows) + ")");
    }

    // Dimensions: "time" × "channel"
    DimensionDescriptor dims{{{"time", num_rows},
                              {"channel", num_cols}}};
    if (!column_names.empty()) {
        dims.setColumnNames(std::move(column_names));
    }

    RowDescriptor rows = RowDescriptor::fromTimeIndices(
            std::move(time_storage), time_frame);

    auto storage = makeStorage(data, {num_rows, num_cols});

    return TensorData{std::move(dims), std::move(rows),
                      std::move(storage), std::move(time_frame)};
}

// =============================================================================
// Factory: createTimeSeries2D (arma::fmat overload)
// =============================================================================

TensorData TensorData::createTimeSeries2D(
        arma::fmat matrix,
        std::shared_ptr<TimeIndexStorage> time_storage,
        std::shared_ptr<TimeFrame> time_frame,
        std::vector<std::string> column_names) {
    if (!time_storage) {
        throw std::invalid_argument(
                "TensorData::createTimeSeries2D: time_storage must not be null");
    }
    auto const n_rows = static_cast<std::size_t>(matrix.n_rows);
    auto const n_cols = static_cast<std::size_t>(matrix.n_cols);
    if (time_storage->size() != n_rows) {
        throw std::invalid_argument(
                "TensorData::createTimeSeries2D: time_storage size (" +
                std::to_string(time_storage->size()) +
                ") must match matrix rows (" + std::to_string(n_rows) + ")");
    }

    DimensionDescriptor dims{{{"time", n_rows},
                              {"channel", n_cols}}};
    if (!column_names.empty()) {
        dims.setColumnNames(std::move(column_names));
    }

    RowDescriptor rows = RowDescriptor::fromTimeIndices(
            std::move(time_storage), time_frame);

    auto storage = TensorStorageWrapper{ArmadilloTensorStorage{std::move(matrix)}};

    return TensorData{std::move(dims), std::move(rows),
                      std::move(storage), std::move(time_frame)};
}

// =============================================================================
// Factory: createTimeSeries2DFromStorage
// =============================================================================

TensorData TensorData::createTimeSeries2DFromStorage(
        TensorStorageWrapper storage,
        std::shared_ptr<TimeIndexStorage> time_storage,
        std::shared_ptr<TimeFrame> time_frame,
        std::vector<std::string> column_names) {
    if (!time_storage) {
        throw std::invalid_argument(
                "TensorData::createTimeSeries2DFromStorage: time_storage must not be null");
    }
    auto const shape = storage.shape();
    if (shape.size() != 2) {
        throw std::invalid_argument(
                "TensorData::createTimeSeries2DFromStorage: storage must be 2D (got " +
                std::to_string(shape.size()) + "D)");
    }
    auto const n_rows = shape[0];
    auto const n_cols = shape[1];
    if (time_storage->size() != n_rows) {
        throw std::invalid_argument(
                "TensorData::createTimeSeries2DFromStorage: time_storage size (" +
                std::to_string(time_storage->size()) +
                ") must match storage rows (" + std::to_string(n_rows) + ")");
    }

    DimensionDescriptor dims{{{"time", n_rows},
                              {"channel", n_cols}}};
    if (!column_names.empty()) {
        dims.setColumnNames(std::move(column_names));
    }

    RowDescriptor rows = RowDescriptor::fromTimeIndices(
            std::move(time_storage), time_frame);

    return TensorData{std::move(dims), std::move(rows),
                      std::move(storage), std::move(time_frame)};
}

// =============================================================================
// Factory: createFromIntervals
// =============================================================================

TensorData TensorData::createFromIntervals(
        std::vector<float> const & data,
        std::size_t num_rows,
        std::size_t num_cols,
        std::vector<TimeFrameInterval> intervals,
        std::shared_ptr<TimeFrame> time_frame,
        std::vector<std::string> column_names) {
    if (intervals.size() != num_rows) {
        throw std::invalid_argument(
                "TensorData::createFromIntervals: intervals size (" +
                std::to_string(intervals.size()) +
                ") must match num_rows (" + std::to_string(num_rows) + ")");
    }

    DimensionDescriptor dims{{{"row", num_rows},
                              {"channel", num_cols}}};
    if (!column_names.empty()) {
        dims.setColumnNames(std::move(column_names));
    }

    RowDescriptor rows = RowDescriptor::fromIntervals(
            std::move(intervals), time_frame);

    auto storage = makeStorage(data, {num_rows, num_cols});

    return TensorData{std::move(dims), std::move(rows),
                      std::move(storage), std::move(time_frame)};
}

// =============================================================================
// Factory: createFromIntervals (arma::fmat overload)
// =============================================================================

TensorData TensorData::createFromIntervals(
        arma::fmat matrix,
        std::vector<TimeFrameInterval> intervals,
        std::shared_ptr<TimeFrame> time_frame,
        std::vector<std::string> column_names) {
    auto const num_rows = static_cast<std::size_t>(matrix.n_rows);
    auto const num_cols = static_cast<std::size_t>(matrix.n_cols);

    if (intervals.size() != num_rows) {
        throw std::invalid_argument(
                "TensorData::createFromIntervals: intervals size (" +
                std::to_string(intervals.size()) +
                ") must match matrix rows (" + std::to_string(num_rows) + ")");
    }

    DimensionDescriptor dims{{{"row", num_rows},
                              {"channel", num_cols}}};
    if (!column_names.empty()) {
        dims.setColumnNames(std::move(column_names));
    }

    RowDescriptor rows = RowDescriptor::fromIntervals(
            std::move(intervals), time_frame);

    auto storage = TensorStorageWrapper{ArmadilloTensorStorage{std::move(matrix)}};

    return TensorData{std::move(dims), std::move(rows),
                      std::move(storage), std::move(time_frame)};
}

// =============================================================================
// Factory: createFromIntervalsFromStorage
// =============================================================================

TensorData TensorData::createFromIntervalsFromStorage(
        TensorStorageWrapper storage,
        std::vector<TimeFrameInterval> intervals,
        std::shared_ptr<TimeFrame> time_frame,
        std::vector<std::string> column_names) {
    auto const shape = storage.shape();
    if (shape.size() != 2) {
        throw std::invalid_argument(
                "TensorData::createFromIntervalsFromStorage: storage must be 2D (got " +
                std::to_string(shape.size()) + "D)");
    }

    auto const num_rows = shape[0];
    auto const num_cols = shape[1];
    if (intervals.size() != num_rows) {
        throw std::invalid_argument(
                "TensorData::createFromIntervalsFromStorage: intervals size (" +
                std::to_string(intervals.size()) +
                ") must match storage rows (" + std::to_string(num_rows) + ")");
    }

    DimensionDescriptor dims{{{"row", num_rows},
                              {"channel", num_cols}}};
    if (!column_names.empty()) {
        dims.setColumnNames(std::move(column_names));
    }

    RowDescriptor rows = RowDescriptor::fromIntervals(
            std::move(intervals), time_frame);

    return TensorData{std::move(dims), std::move(rows),
                      std::move(storage), std::move(time_frame)};
}

// =============================================================================
// Factory: createND
// =============================================================================

TensorData TensorData::createND(
        std::vector<float> const & data,
        std::vector<AxisDescriptor> const & axes) {
    if (axes.empty()) {
        throw std::invalid_argument(
                "TensorData::createND: axes must not be empty");
    }

    // Build shape vector
    std::vector<std::size_t> shape_vec;
    shape_vec.reserve(axes.size());
    for (auto const & ax: axes) {
        shape_vec.push_back(ax.size);
    }

    DimensionDescriptor dims{axes};
    auto rows = RowDescriptor::ordinal(shape_vec[0]);
    auto storage = makeStorage(data, shape_vec);

    return TensorData{std::move(dims), std::move(rows),
                      std::move(storage), nullptr};
}

// =============================================================================
// Factory: createFromArmadillo (matrix)
// =============================================================================

TensorData TensorData::createFromArmadillo(
        arma::fmat matrix,
        std::vector<std::string> column_names) {
    auto const n_rows = static_cast<std::size_t>(matrix.n_rows);
    auto const n_cols = static_cast<std::size_t>(matrix.n_cols);

    DimensionDescriptor dims{{{"row", n_rows},
                              {"channel", n_cols}}};
    if (!column_names.empty()) {
        dims.setColumnNames(std::move(column_names));
    }

    auto rows = RowDescriptor::ordinal(n_rows);
    auto storage = TensorStorageWrapper{ArmadilloTensorStorage{std::move(matrix)}};

    return TensorData{std::move(dims), std::move(rows),
                      std::move(storage), nullptr};
}

// =============================================================================
// Factory: createFromArmadillo (cube)
// =============================================================================

TensorData TensorData::createFromArmadillo(
        arma::fcube cube,
        std::vector<AxisDescriptor> axes) {
    auto const n_slices = static_cast<std::size_t>(cube.n_slices);
    auto const n_rows = static_cast<std::size_t>(cube.n_rows);
    auto const n_cols = static_cast<std::size_t>(cube.n_cols);

    if (axes.empty()) {
        axes = {
                {"dim0", n_slices},
                {"dim1", n_rows},
                {"dim2", n_cols}};
    }

    DimensionDescriptor dims{axes};
    auto rows = RowDescriptor::ordinal(n_slices);
    auto storage = TensorStorageWrapper{ArmadilloTensorStorage{std::move(cube)}};

    return TensorData{std::move(dims), std::move(rows),
                      std::move(storage), nullptr};
}

// =============================================================================
// Factory: createOrdinal2D
// =============================================================================

TensorData TensorData::createOrdinal2D(
        std::vector<float> const & data,
        std::size_t num_rows,
        std::size_t num_cols,
        std::vector<std::string> column_names) {
    DimensionDescriptor dims{{{"row", num_rows},
                              {"channel", num_cols}}};
    if (!column_names.empty()) {
        dims.setColumnNames(std::move(column_names));
    }

    auto rows = RowDescriptor::ordinal(num_rows);
    auto storage = makeStorage(data, {num_rows, num_cols});

    return TensorData{std::move(dims), std::move(rows),
                      std::move(storage), nullptr};
}

// =============================================================================
// Factory: createOrdinal2DFromStorage
// =============================================================================

TensorData TensorData::createOrdinal2DFromStorage(
        TensorStorageWrapper storage,
        std::shared_ptr<TimeFrame> time_frame,
        std::vector<std::string> column_names) {
    auto const shape = storage.shape();
    if (shape.size() != 2) {
        throw std::invalid_argument(
                "TensorData::createOrdinal2DFromStorage: storage must be 2D (got " +
                std::to_string(shape.size()) + "D)");
    }

    auto const num_rows = shape[0];
    auto const num_cols = shape[1];

    DimensionDescriptor dims{{{"row", num_rows},
                              {"channel", num_cols}}};
    if (!column_names.empty()) {
        dims.setColumnNames(std::move(column_names));
    }

    auto rows = RowDescriptor::ordinal(num_rows);
    return TensorData{std::move(dims), std::move(rows),
                      std::move(storage), std::move(time_frame)};
}

// =============================================================================
// Factory: createFromLazyColumns
// =============================================================================

TensorData TensorData::createFromLazyColumns(
        std::size_t num_rows,
        std::vector<ColumnSource> columns,
        RowDescriptor rows,
        InvalidationWiringFn const & wiring) {
    if (num_rows == 0) {
        throw std::invalid_argument(
                "TensorData::createFromLazyColumns: num_rows must be > 0");
    }
    if (columns.empty()) {
        throw std::invalid_argument(
                "TensorData::createFromLazyColumns: must have at least one column");
    }

    auto const num_cols = columns.size();

    // Extract column names for the DimensionDescriptor
    std::vector<std::string> col_names;
    col_names.reserve(num_cols);
    for (auto const & col: columns) {
        col_names.push_back(col.name);
    }

    // Build dimension descriptor: "row" × "channel" (same convention as other factories)
    DimensionDescriptor dims{{{"row", num_rows},
                              {"channel", num_cols}}};
    dims.setColumnNames(std::move(col_names));

    // Extract time_frame from RowDescriptor if available
    auto time_frame = rows.timeFrame();

    // Build lazy storage
    auto storage = TensorStorageWrapper{
            LazyColumnTensorStorage{num_rows, std::move(columns)}};

    auto tensor = TensorData{std::move(dims), std::move(rows),
                             std::move(storage), std::move(time_frame)};

    // Wire invalidation if a callback was provided
    if (wiring) {
        auto * lazy = tensor._storage.tryGetMutableAs<LazyColumnTensorStorage>();
        if (lazy != nullptr) {
            wiring(*lazy, tensor);
        }
    }

    return tensor;
}

// =============================================================================
// Dimension Queries
// =============================================================================

DimensionDescriptor const & TensorData::dimensions() const noexcept {
    return _dimensions;
}

std::size_t TensorData::ndim() const noexcept {
    return _dimensions.ndim();
}

std::vector<std::size_t> TensorData::shape() const {
    return _dimensions.shape();
}

// =============================================================================
// Row Queries
// =============================================================================

RowDescriptor const & TensorData::rows() const noexcept {
    return _rows;
}

RowType TensorData::rowType() const noexcept {
    return _rows.type();
}

std::size_t TensorData::numRows() const noexcept {
    return _rows.count();
}

// =============================================================================
// Column / Channel Access
// =============================================================================

bool TensorData::hasNamedColumns() const noexcept {
    return _dimensions.hasColumnNames();
}

std::vector<std::string> const & TensorData::columnNames() const noexcept {
    return _dimensions.columnNames();
}

std::size_t TensorData::numColumns() const noexcept {
    if (_dimensions.ndim() < 2) {
        return (_dimensions.ndim() == 1) ? 1 : 0;
    }
    return _dimensions.axis(_dimensions.ndim() - 1).size;
}

std::vector<float> TensorData::getColumn(std::size_t index) const {
    if (!_storage.isValid()) {
        throw std::runtime_error("TensorData::getColumn: tensor has no storage");
    }
    if (index >= numColumns()) {
        throw std::out_of_range(
                "TensorData::getColumn: index " + std::to_string(index) +
                " >= numColumns() " + std::to_string(numColumns()));
    }
    return _storage.getColumn(index);
}

std::vector<float> TensorData::getColumn(std::string_view name) const {
    auto const col_idx = _dimensions.findColumn(name);
    if (!col_idx.has_value()) {
        throw std::invalid_argument(
                "TensorData::getColumn: column '" + std::string(name) +
                "' not found");
    }
    return getColumn(col_idx.value());
}

// =============================================================================
// Element Access
// =============================================================================

float TensorData::at(std::span<std::size_t const> indices) const {
    if (!_storage.isValid()) {
        throw std::runtime_error("TensorData::at: tensor has no storage");
    }
    return _storage.getValueAt(indices);
}

std::vector<float> TensorData::row(std::size_t index) const {
    if (!_storage.isValid()) {
        throw std::runtime_error("TensorData::row: tensor has no storage");
    }
    if (_dimensions.ndim() == 0) {
        throw std::logic_error("TensorData::row: scalar tensor has no rows");
    }
    if (index >= _dimensions.axis(0).size) {
        throw std::out_of_range(
                "TensorData::row: index " + std::to_string(index) +
                " >= axis(0).size " + std::to_string(_dimensions.axis(0).size));
    }
    return _storage.sliceAlongAxis(0, index);
}

std::span<float const> TensorData::flatData() const {
    if (!_storage.isValid()) {
        throw std::runtime_error("TensorData::flatData: tensor has no storage");
    }
    return _storage.flatData();
}

std::vector<float> TensorData::materializeFlat() const {
    if (!_storage.isValid()) {
        return {};
    }

    // For row-major storage (Dense), we can just copy flatData
    if (_storage.isContiguous() &&
        _storage.getStorageType() != TensorStorageType::Armadillo) {
        auto span = _storage.flatData();
        return {span.begin(), span.end()};
    }

    // For Armadillo (column-major) or non-contiguous storage,
    // reconstruct row-major by element-wise access
    auto const total = _dimensions.totalElements();
    auto const s = _dimensions.shape();
    std::vector<float> result(total);

    std::vector<std::size_t> indices(s.size(), 0);
    for (std::size_t flat = 0; flat < total; ++flat) {
        result[flat] = _storage.getValueAt(indices);

        // Increment multi-dimensional index (row-major order)
        for (auto dim = static_cast<int>(s.size()) - 1; dim >= 0; --dim) {
            ++indices[static_cast<std::size_t>(dim)];
            if (indices[static_cast<std::size_t>(dim)] < s[static_cast<std::size_t>(dim)]) {
                break;
            }
            indices[static_cast<std::size_t>(dim)] = 0;
        }
    }

    return result;
}

// =============================================================================
// Backend Conversion
// =============================================================================

TensorData TensorData::materialize() const {
    if (!_storage.isValid()) {
        return *this;// empty tensor
    }

    auto const s = _dimensions.shape();
    auto data = materializeFlat();

    // Build axes from current dimensions
    std::vector<AxisDescriptor> axes;
    axes.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        axes.push_back(_dimensions.axis(i));
    }

    DimensionDescriptor dims{axes};
    if (_dimensions.hasColumnNames()) {
        dims.setColumnNames(
                std::vector<std::string>(_dimensions.columnNames().begin(),
                                         _dimensions.columnNames().end()));
    }

    auto storage = makeStorage(data, s);

    return TensorData{std::move(dims), _rows, std::move(storage), _time_frame};
}

TensorData TensorData::toArmadillo() const {
    if (_dimensions.ndim() > 3) {
        throw std::logic_error(
                "TensorData::toArmadillo: ndim() = " +
                std::to_string(_dimensions.ndim()) + " > 3; Armadillo only supports ≤3D");
    }

    // Already Armadillo-backed? Return shallow copy.
    if (_storage.isValid() &&
        _storage.getStorageType() == TensorStorageType::Armadillo) {
        return *this;
    }

#ifdef TENSOR_BACKEND_LIBTORCH
    // Fast path: LibTorch 2D → Armadillo without intermediate std::vector.
    // LibTorch is row-major, Armadillo is column-major.  We read directly
    // from the torch::Tensor data pointer into the arma::fmat, performing
    // the transpose in a single pass and avoiding the 128 MB intermediate
    // flat vector that materialize() → materializeFlat() would create.
    if (_storage.isValid() &&
        _storage.getStorageType() == TensorStorageType::LibTorch &&
        _dimensions.ndim() == 2) {

        auto const * lt = _storage.getAsChecked<LibTorchTensorStorage>(
                TensorStorageType::LibTorch);
        if (lt && lt->isCpu()) {
            auto t = lt->tensor().contiguous();
            auto const * src = t.data_ptr<float>();
            auto const num_rows = static_cast<std::size_t>(t.size(0));
            auto const num_cols = static_cast<std::size_t>(t.size(1));

            arma::fmat mat(num_rows, num_cols);
            for (std::size_t r = 0; r < num_rows; ++r) {
                for (std::size_t c = 0; c < num_cols; ++c) {
                    mat(r, c) = src[r * num_cols + c];
                }
            }

            auto storage = TensorStorageWrapper{
                    ArmadilloTensorStorage{std::move(mat)}};

            // Preserve dimensions and metadata
            auto s = _dimensions.shape();
            std::vector<AxisDescriptor> axes;
            axes.reserve(s.size());
            for (std::size_t i = 0; i < s.size(); ++i) {
                axes.push_back(_dimensions.axis(i));
            }
            DimensionDescriptor dims{axes};
            if (_dimensions.hasColumnNames()) {
                dims.setColumnNames(
                        std::vector<std::string>(_dimensions.columnNames().begin(),
                                                 _dimensions.columnNames().end()));
            }

            return TensorData{std::move(dims), _rows, std::move(storage), _time_frame};
        }
    }
#endif

    // Generic path: materialize via flat vector (for Dense, Lazy, Mmap, etc.)
    return materialize();
}

arma::fmat const & TensorData::asArmadilloMatrix() const {
    if (!_storage.isValid()) {
        throw std::logic_error("TensorData::asArmadilloMatrix: empty tensor");
    }
    auto const * arma_storage = _storage.getAsChecked<ArmadilloTensorStorage>(
            TensorStorageType::Armadillo);
    if (!arma_storage) {
        throw std::logic_error(
                "TensorData::asArmadilloMatrix: storage is not Armadillo-backed "
                "(use toArmadillo() first)");
    }
    return arma_storage->matrix();// throws internally if not 2D
}

arma::fcube const & TensorData::asArmadilloCube() const {
    if (!_storage.isValid()) {
        throw std::logic_error("TensorData::asArmadilloCube: empty tensor");
    }
    auto const * arma_storage = _storage.getAsChecked<ArmadilloTensorStorage>(
            TensorStorageType::Armadillo);
    if (!arma_storage) {
        throw std::logic_error(
                "TensorData::asArmadilloCube: storage is not Armadillo-backed "
                "(use toArmadillo() first)");
    }
    return arma_storage->cube();// throws internally if not 3D
}

// =============================================================================
// Backend Conversion: LibTorch
// =============================================================================

#ifdef TENSOR_BACKEND_LIBTORCH
TensorData TensorData::toLibTorch() const {
    // Already LibTorch-backed? Return shallow copy.
    if (_storage.isValid() &&
        _storage.getStorageType() == TensorStorageType::LibTorch) {
        return *this;
    }

    if (!_storage.isValid()) {
        throw std::logic_error("TensorData::toLibTorch: empty tensor");
    }

    auto const s = _dimensions.shape();

    // Fast path: Armadillo → strided view → clone (single contiguous memcpy).
    // Avoids the element-by-element virtual dispatch in materializeFlat().
    if (auto const * arma = _storage.getAsChecked<ArmadilloTensorStorage>(
                TensorStorageType::Armadillo)) {
        torch::Tensor t;

        if (arma->ndim() == 1) {
            // 1D: layout is identical for column-major and row-major
            auto const & vec = arma->vector();
            t = torch::from_blob(
                        const_cast<float *>(vec.memptr()),
                        {static_cast<int64_t>(vec.n_elem)},
                        torch::kFloat32)
                        .clone();
        } else if (arma->ndim() == 2) {
            // 2D: create a strided view with column-major strides, then
            // call contiguous() to produce a row-major tensor that owns
            // its own memory.  (clone() preserves strides, so we need
            // contiguous() explicitly.)
            auto const & mat = arma->matrix();
            t = torch::from_blob(
                        const_cast<float *>(mat.memptr()),
                        {static_cast<int64_t>(mat.n_rows),
                         static_cast<int64_t>(mat.n_cols)},
                        {1, static_cast<int64_t>(mat.n_rows)},
                        torch::kFloat32)
                        .contiguous();
        } else {
            // 3D (fcube): fall through to generic path below
            goto generic_path;// NOLINT(cppcoreguidelines-avoid-goto)
        }

        // Preserve dimensions, rows, timeframe
        std::vector<AxisDescriptor> axes;
        axes.reserve(s.size());
        for (std::size_t i = 0; i < s.size(); ++i) {
            axes.push_back(_dimensions.axis(i));
        }
        DimensionDescriptor dims{axes};
        if (_dimensions.hasColumnNames()) {
            dims.setColumnNames(
                    std::vector<std::string>(_dimensions.columnNames().begin(),
                                             _dimensions.columnNames().end()));
        }

        return TensorData{std::move(dims), _rows,
                          TensorStorageWrapper{LibTorchTensorStorage{std::move(t)}},
                          _time_frame};
    }

generic_path:
    // Generic path: materialize to row-major flat data, then wrap in torch::Tensor
    auto flat = materializeFlat();

    auto torch_storage = LibTorchTensorStorage::fromFlatData(flat, s);

    // Preserve dimensions, rows, timeframe
    std::vector<AxisDescriptor> axes;
    axes.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        axes.push_back(_dimensions.axis(i));
    }
    DimensionDescriptor dims{axes};
    if (_dimensions.hasColumnNames()) {
        dims.setColumnNames(
                std::vector<std::string>(_dimensions.columnNames().begin(),
                                         _dimensions.columnNames().end()));
    }

    return TensorData{std::move(dims), _rows,
                      TensorStorageWrapper{std::move(torch_storage)},
                      _time_frame};
}

TensorData TensorData::toLibTorchStrided() const {
    // Already LibTorch-backed? Return shallow copy (same as toLibTorch).
    if (_storage.isValid() &&
        _storage.getStorageType() == TensorStorageType::LibTorch) {
        return *this;
    }

    if (!_storage.isValid()) {
        throw std::logic_error("TensorData::toLibTorchStrided: empty tensor");
    }

    // Only the Armadillo 2D case benefits from a strided (non-contiguous)
    // tensor.  The caller can then do .to(device).contiguous() to perform
    // the column-major → row-major transpose on GPU bandwidth.
    if (auto const * arma = _storage.getAsChecked<ArmadilloTensorStorage>(
                TensorStorageType::Armadillo);
        arma && arma->ndim() == 2) {

        auto const & mat = arma->matrix();

        // Non-owning view with column-major strides.  The returned
        // TensorData keeps _storage alive (shared_ptr in wrapper),
        // so the Armadillo buffer remains valid.
        auto t = torch::from_blob(
                const_cast<float *>(mat.memptr()),
                {static_cast<int64_t>(mat.n_rows),
                 static_cast<int64_t>(mat.n_cols)},
                {1, static_cast<int64_t>(mat.n_rows)},
                torch::kFloat32);

        // Build output TensorData that shares the Armadillo memory
        auto s = _dimensions.shape();
        std::vector<AxisDescriptor> axes;
        axes.reserve(s.size());
        for (std::size_t i = 0; i < s.size(); ++i) {
            axes.push_back(_dimensions.axis(i));
        }
        DimensionDescriptor dims{axes};
        if (_dimensions.hasColumnNames()) {
            dims.setColumnNames(
                    std::vector<std::string>(_dimensions.columnNames().begin(),
                                             _dimensions.columnNames().end()));
        }

        return TensorData{std::move(dims), _rows,
                          TensorStorageWrapper{LibTorchTensorStorage{std::move(t)}},
                          _time_frame};
    }

    // Fallback: 1D, 3D, or non-Armadillo storage → use toLibTorch() which
    // produces a contiguous tensor.
    return toLibTorch();
}

#endif// TENSOR_BACKEND_LIBTORCH

// =============================================================================
// Mutation
// =============================================================================

void TensorData::setData(std::vector<float> const & data,
                         std::vector<std::size_t> const & new_shape) {
    if (new_shape.empty()) {
        throw std::invalid_argument("TensorData::setData: shape must not be empty");
    }

    _storage = makeStorage(data, new_shape);

    // Rebuild dimensions from shape (use generic axis names)
    std::vector<AxisDescriptor> axes;
    axes.reserve(new_shape.size());
    for (std::size_t i = 0; i < new_shape.size(); ++i) {
        axes.push_back({"dim" + std::to_string(i), new_shape[i]});
    }
    _dimensions = DimensionDescriptor{axes};

    // Update row descriptor to ordinal
    _rows = RowDescriptor::ordinal(new_shape[0]);

    notifyObservers();
}

void TensorData::setData(std::vector<float> && data,
                         std::vector<std::size_t> const & new_shape) {
    // For now, delegating to the const-ref version since makeStorage takes const ref.
    // The Dense storage constructor does accept by value, but we'd need to refactor
    // makeStorage to accept rvalue. Keeping this simple for initial implementation.
    setData(static_cast<std::vector<float> const &>(data), new_shape);
}

// =============================================================================
// Column Mutation (LazyColumnTensorStorage only)
// =============================================================================

std::size_t TensorData::appendColumn(std::string name, ColumnProviderFn provider) {
    auto * lazy = _storage.tryGetMutableAs<LazyColumnTensorStorage>();
    if (lazy == nullptr) {
        throw std::logic_error(
                "TensorData::appendColumn: only supported for LazyColumnTensorStorage");
    }

    // Append to storage
    auto const new_col_index = lazy->appendColumn(std::move(name), std::move(provider));

    // Update dimension descriptor: resize last axis + update column names
    auto const new_num_cols = lazy->numColumns();
    auto col_axis_index = _dimensions.findAxis("channel");
    if (col_axis_index.has_value()) {
        _dimensions.setAxisSize(*col_axis_index, new_num_cols);
    }

    // Rebuild column names from storage (authoritative source)
    _dimensions.setColumnNames(lazy->columnNames());

    notifyObservers();
    return new_col_index;
}

void TensorData::removeColumn(std::size_t col) {
    auto * lazy = _storage.tryGetMutableAs<LazyColumnTensorStorage>();
    if (lazy == nullptr) {
        throw std::logic_error(
                "TensorData::removeColumn: only supported for LazyColumnTensorStorage");
    }

    // Remove from storage (validates col index and > 1 column)
    lazy->removeColumn(col);

    // Update dimension descriptor: resize last axis + update column names
    auto const new_num_cols = lazy->numColumns();
    auto col_axis_index = _dimensions.findAxis("channel");
    if (col_axis_index.has_value()) {
        _dimensions.setAxisSize(*col_axis_index, new_num_cols);
    }

    // Rebuild column names from storage
    _dimensions.setColumnNames(lazy->columnNames());

    notifyObservers();
}

// =============================================================================
// Row Mutation
// =============================================================================

namespace {

/**
 * @brief Delegate row insertion to the appropriate storage backend
 *
 * Tries ArmadilloTensorStorage first, then DenseTensorStorage.
 * Throws std::logic_error if neither matches.
 */
void insertRowInStorage(TensorStorageWrapper & storage,
                        std::size_t index,
                        std::span<float const> row_data) {
    if (auto * arma = storage.tryGetMutableAs<ArmadilloTensorStorage>()) {
        arma->insertRow(index, row_data);
        return;
    }
    if (auto * dense = storage.tryGetMutableAs<DenseTensorStorage>()) {
        dense->insertRow(index, row_data);
        return;
    }
    throw std::logic_error(
            "TensorData row mutation: only supported for "
            "ArmadilloTensorStorage or DenseTensorStorage");
}

}// anonymous namespace

void TensorData::appendRow(std::span<float const> row_data) {
    if (ndim() != 2) {
        throw std::logic_error(
                "TensorData::appendRow: only supported for 2D tensors, "
                "current ndim=" +
                std::to_string(ndim()));
    }
    if (_rows.type() == RowType::TimeFrameIndex) {
        throw std::logic_error(
                "TensorData::appendRow: not supported for TimeFrameIndex rows "
                "(time index storage is immutable)");
    }

    auto const insert_index = numRows();
    insertRowInStorage(_storage, insert_index, row_data);

    // Update row descriptor
    if (_rows.type() == RowType::Ordinal) {
        _rows.setOrdinalCount(insert_index + 1);
    }
    // Interval rows: caller must use the overload that takes an interval

    // Update dimension descriptor (axis 0 size)
    _dimensions.setAxisSize(0, insert_index + 1);

    notifyObservers();
}

void TensorData::appendRow(std::span<float const> row_data, TimeFrameInterval interval) {
    if (_rows.type() != RowType::Interval) {
        throw std::logic_error(
                "TensorData::appendRow(interval): row type must be Interval");
    }
    if (ndim() != 2) {
        throw std::logic_error(
                "TensorData::appendRow: only supported for 2D tensors, "
                "current ndim=" +
                std::to_string(ndim()));
    }

    auto const insert_index = numRows();
    insertRowInStorage(_storage, insert_index, row_data);

    _rows.appendInterval(interval);
    _dimensions.setAxisSize(0, insert_index + 1);

    notifyObservers();
}

void TensorData::insertRow(std::size_t index, std::span<float const> row_data) {
    if (ndim() != 2) {
        throw std::logic_error(
                "TensorData::insertRow: only supported for 2D tensors, "
                "current ndim=" +
                std::to_string(ndim()));
    }
    if (_rows.type() == RowType::TimeFrameIndex) {
        throw std::logic_error(
                "TensorData::insertRow: not supported for TimeFrameIndex rows "
                "(time index storage is immutable)");
    }
    if (index > numRows()) {
        throw std::out_of_range(
                "TensorData::insertRow: index " + std::to_string(index) +
                " > numRows " + std::to_string(numRows()));
    }

    auto const new_row_count = numRows() + 1;
    insertRowInStorage(_storage, index, row_data);

    // Update row descriptor
    if (_rows.type() == RowType::Ordinal) {
        _rows.setOrdinalCount(new_row_count);
    }

    // Update dimension descriptor (axis 0 size)
    _dimensions.setAxisSize(0, new_row_count);

    notifyObservers();
}

void TensorData::insertRow(std::size_t index, std::span<float const> row_data,
                           TimeFrameInterval interval) {
    if (_rows.type() != RowType::Interval) {
        throw std::logic_error(
                "TensorData::insertRow(interval): row type must be Interval");
    }
    if (ndim() != 2) {
        throw std::logic_error(
                "TensorData::insertRow: only supported for 2D tensors, "
                "current ndim=" +
                std::to_string(ndim()));
    }
    if (index > numRows()) {
        throw std::out_of_range(
                "TensorData::insertRow: index " + std::to_string(index) +
                " > numRows " + std::to_string(numRows()));
    }

    auto const new_row_count = numRows() + 1;
    insertRowInStorage(_storage, index, row_data);

    _rows.insertInterval(index, interval);
    _dimensions.setAxisSize(0, new_row_count);

    notifyObservers();
}

void TensorData::setRow(std::size_t index, std::span<float const> row_data) {
    if (ndim() != 2) {
        throw std::logic_error(
                "TensorData::setRow: only supported for 2D tensors, "
                "current ndim=" +
                std::to_string(ndim()));
    }
    if (index >= numRows()) {
        throw std::out_of_range(
                "TensorData::setRow: index " + std::to_string(index) +
                " >= numRows " + std::to_string(numRows()));
    }

    if (auto * arma = _storage.tryGetMutableAs<ArmadilloTensorStorage>()) {
        arma->setRow(index, row_data);
    } else if (auto * dense = _storage.tryGetMutableAs<DenseTensorStorage>()) {
        dense->setRow(index, row_data);
    } else {
        throw std::logic_error(
                "TensorData::setRow: only supported for "
                "ArmadilloTensorStorage or DenseTensorStorage");
    }

    notifyObservers();
}

void TensorData::upsertRows(
        std::vector<std::pair<int, std::vector<float>>> const & frame_rows,
        std::shared_ptr<TimeFrame> time_frame) {
    if (frame_rows.empty()) {
        throw std::invalid_argument("TensorData::upsertRows: frame_rows must not be empty");
    }
    if (!time_frame) {
        throw std::invalid_argument("TensorData::upsertRows: time_frame must not be null");
    }

    auto const new_cols = frame_rows.front().second.size();
    for (auto const & [frame, vec]: frame_rows) {
        if (vec.size() != new_cols) {
            throw std::invalid_argument(
                    "TensorData::upsertRows: inconsistent row sizes (" +
                    std::to_string(vec.size()) + " vs " +
                    std::to_string(new_cols) + ")");
        }
    }

    // Build merged frame→data map: start from existing rows, overlay new ones
    std::map<int, std::vector<float>> merged;

    if (_storage.isValid() && numRows() > 0) {
        if (ndim() != 2) {
            throw std::logic_error(
                    "TensorData::upsertRows: existing tensor must be 2D, "
                    "current ndim=" +
                    std::to_string(ndim()));
        }
        if (numColumns() != new_cols) {
            throw std::invalid_argument(
                    "TensorData::upsertRows: new row width (" +
                    std::to_string(new_cols) +
                    ") != existing numColumns (" +
                    std::to_string(numColumns()) + ")");
        }

        // Extract existing frame→data from current storage
        if (_rows.type() == RowType::TimeFrameIndex) {
            auto const & ts = _rows.timeStorage();
            for (std::size_t i = 0; i < numRows(); ++i) {
                auto tfi = ts.getTimeFrameIndexAt(i);
                merged[static_cast<int>(tfi.getValue())] = row(i);
            }
        } else {
            // Ordinal rows — treat row index as frame index
            for (std::size_t i = 0; i < numRows(); ++i) {
                merged[static_cast<int>(i)] = row(i);
            }
        }
    }

    // Overlay new rows (overwrites existing frames, inserts new)
    for (auto const & [frame, vec]: frame_rows) {
        merged[frame] = vec;
    }

    // Flatten into sorted arrays
    std::vector<TimeFrameIndex> time_indices;
    std::vector<float> flat_data;
    time_indices.reserve(merged.size());
    flat_data.reserve(merged.size() * new_cols);

    for (auto const & [frame, vec]: merged) {
        time_indices.emplace_back(frame);
        flat_data.insert(flat_data.end(), vec.begin(), vec.end());
    }

    auto const n_rows = time_indices.size();
    auto time_storage = TimeIndexStorageFactory::createFromTimeIndices(
            std::move(time_indices));

    // Rebuild this tensor in-place
    DimensionDescriptor dims{{{"time", n_rows}, {"channel", new_cols}}};

    // Preserve column names if they match
    if (hasNamedColumns() && columnNames().size() == new_cols) {
        dims.setColumnNames(columnNames());
    }

    _rows = RowDescriptor::fromTimeIndices(std::move(time_storage), time_frame);
    _dimensions = std::move(dims);
    _storage = makeStorage(flat_data, {n_rows, new_cols});
    _time_frame = std::move(time_frame);

    notifyObservers();
}

// =============================================================================
// Storage Access
// =============================================================================

TensorStorageWrapper const & TensorData::storage() const noexcept {
    return _storage;
}

bool TensorData::isContiguous() const noexcept {
    if (!_storage.isValid()) {
        return false;
    }
    return _storage.isContiguous();
}

bool TensorData::isEmpty() const noexcept {
    return !_storage.isValid();
}

// =============================================================================
// TimeFrame
// =============================================================================

void TensorData::setTimeFrame(std::shared_ptr<TimeFrame> tf) {
    _time_frame = std::move(tf);
}

std::shared_ptr<TimeFrame> TensorData::getTimeFrame() const noexcept {
    return _time_frame;
}

// =============================================================================
// Column Names Mutation
// =============================================================================

void TensorData::setColumnNames(std::vector<std::string> names) {
    _dimensions.setColumnNames(std::move(names));
}
