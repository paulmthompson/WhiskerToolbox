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
#include "Tensors/storage/TensorStorageWrapper.hpp"

#ifdef TENSOR_BACKEND_LIBTORCH
#include "Tensors/storage/LibTorchTensorStorage.hpp"
#endif

#include "TimeFrame/TimeIndexStorage.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>

// =============================================================================
// Default / Special Members
// =============================================================================

TensorData::TensorData()
    : _dimensions{}
    , _rows{RowDescriptor::ordinal(0)}
    , _storage{}
    , _time_frame{nullptr}
{
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
    : _dimensions{std::move(dimensions)}
    , _rows{std::move(rows)}
    , _storage{std::move(storage)}
    , _time_frame{std::move(time_frame)}
{
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
                                  std::vector<std::size_t> const & shape)
{
    auto const ndim = shape.size();

    if (ndim == 0) {
        throw std::invalid_argument(
            "TensorData: cannot create storage with zero dimensions");
    }

    // Validate total element count
    std::size_t total = 1;
    for (auto s : shape) {
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

} // anonymous namespace

// =============================================================================
// Factory: createTimeSeries2D
// =============================================================================

TensorData TensorData::createTimeSeries2D(
    std::vector<float> const & data,
    std::size_t num_rows,
    std::size_t num_cols,
    std::shared_ptr<TimeIndexStorage> time_storage,
    std::shared_ptr<TimeFrame> time_frame,
    std::vector<std::string> column_names)
{
    if (!time_storage) {
        throw std::invalid_argument(
            "TensorData::createTimeSeries2D: time_storage must not be null");
    }
    if (!time_frame) {
        throw std::invalid_argument(
            "TensorData::createTimeSeries2D: time_frame must not be null");
    }
    if (time_storage->size() != num_rows) {
        throw std::invalid_argument(
            "TensorData::createTimeSeries2D: time_storage size (" +
            std::to_string(time_storage->size()) +
            ") must match num_rows (" + std::to_string(num_rows) + ")");
    }

    // Dimensions: "time" × "channel"
    DimensionDescriptor dims{{
        {"time", num_rows},
        {"channel", num_cols}
    }};
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
// Factory: createFromIntervals
// =============================================================================

TensorData TensorData::createFromIntervals(
    std::vector<float> const & data,
    std::size_t num_rows,
    std::size_t num_cols,
    std::vector<TimeFrameInterval> intervals,
    std::shared_ptr<TimeFrame> time_frame,
    std::vector<std::string> column_names)
{
    if (!time_frame) {
        throw std::invalid_argument(
            "TensorData::createFromIntervals: time_frame must not be null");
    }
    if (intervals.size() != num_rows) {
        throw std::invalid_argument(
            "TensorData::createFromIntervals: intervals size (" +
            std::to_string(intervals.size()) +
            ") must match num_rows (" + std::to_string(num_rows) + ")");
    }

    DimensionDescriptor dims{{
        {"row", num_rows},
        {"channel", num_cols}
    }};
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
// Factory: createND
// =============================================================================

TensorData TensorData::createND(
    std::vector<float> const & data,
    std::vector<AxisDescriptor> axes)
{
    if (axes.empty()) {
        throw std::invalid_argument(
            "TensorData::createND: axes must not be empty");
    }

    // Build shape vector
    std::vector<std::size_t> shape_vec;
    shape_vec.reserve(axes.size());
    for (auto const & ax : axes) {
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
    std::vector<std::string> column_names)
{
    auto const n_rows = static_cast<std::size_t>(matrix.n_rows);
    auto const n_cols = static_cast<std::size_t>(matrix.n_cols);

    DimensionDescriptor dims{{
        {"row", n_rows},
        {"channel", n_cols}
    }};
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
    std::vector<AxisDescriptor> axes)
{
    auto const n_slices = static_cast<std::size_t>(cube.n_slices);
    auto const n_rows = static_cast<std::size_t>(cube.n_rows);
    auto const n_cols = static_cast<std::size_t>(cube.n_cols);

    if (axes.empty()) {
        axes = {
            {"dim0", n_slices},
            {"dim1", n_rows},
            {"dim2", n_cols}
        };
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
    std::vector<std::string> column_names)
{
    DimensionDescriptor dims{{
        {"row", num_rows},
        {"channel", num_cols}
    }};
    if (!column_names.empty()) {
        dims.setColumnNames(std::move(column_names));
    }

    auto rows = RowDescriptor::ordinal(num_rows);
    auto storage = makeStorage(data, {num_rows, num_cols});

    return TensorData{std::move(dims), std::move(rows),
                         std::move(storage), nullptr};
}

// =============================================================================
// Factory: createFromTorch (LibTorch)
// =============================================================================

#ifdef TENSOR_BACKEND_LIBTORCH
TensorData TensorData::createFromTorch(
    torch::Tensor tensor,
    std::vector<AxisDescriptor> axes)
{
    if (!tensor.defined()) {
        throw std::invalid_argument(
            "TensorData::createFromTorch: tensor must be defined");
    }

    // Convert to float32 if needed (e.g., from double inference output)
    if (tensor.scalar_type() != torch::kFloat32) {
        tensor = tensor.to(torch::kFloat32);
    }

    auto const nd = tensor.dim();
    if (nd == 0) {
        throw std::invalid_argument(
            "TensorData::createFromTorch: scalar tensors (0-dim) not supported");
    }

    // Auto-generate axis descriptors if not provided
    if (axes.empty()) {
        axes.reserve(static_cast<std::size_t>(nd));
        for (int d = 0; d < nd; ++d) {
            axes.push_back({"dim" + std::to_string(d),
                            static_cast<std::size_t>(tensor.size(d))});
        }
    }

    if (static_cast<int>(axes.size()) != nd) {
        throw std::invalid_argument(
            "TensorData::createFromTorch: axes count (" +
            std::to_string(axes.size()) +
            ") doesn't match tensor dims (" + std::to_string(nd) + ")");
    }

    DimensionDescriptor dims{axes};
    auto rows = RowDescriptor::ordinal(static_cast<std::size_t>(tensor.size(0)));
    auto storage = TensorStorageWrapper{LibTorchTensorStorage{std::move(tensor)}};

    return TensorData{std::move(dims), std::move(rows),
                         std::move(storage), nullptr};
}
#endif // TENSOR_BACKEND_LIBTORCH

// =============================================================================
// Dimension Queries
// =============================================================================

DimensionDescriptor const & TensorData::dimensions() const noexcept
{
    return _dimensions;
}

std::size_t TensorData::ndim() const noexcept
{
    return _dimensions.ndim();
}

std::vector<std::size_t> TensorData::shape() const
{
    return _dimensions.shape();
}

// =============================================================================
// Row Queries
// =============================================================================

RowDescriptor const & TensorData::rows() const noexcept
{
    return _rows;
}

RowType TensorData::rowType() const noexcept
{
    return _rows.type();
}

std::size_t TensorData::numRows() const noexcept
{
    return _rows.count();
}

// =============================================================================
// Column / Channel Access
// =============================================================================

bool TensorData::hasNamedColumns() const noexcept
{
    return _dimensions.hasColumnNames();
}

std::vector<std::string> const & TensorData::columnNames() const noexcept
{
    return _dimensions.columnNames();
}

std::size_t TensorData::numColumns() const noexcept
{
    if (_dimensions.ndim() < 2) {
        return (_dimensions.ndim() == 1) ? 1 : 0;
    }
    return _dimensions.axis(_dimensions.ndim() - 1).size;
}

std::vector<float> TensorData::getColumn(std::size_t index) const
{
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

std::vector<float> TensorData::getColumn(std::string_view name) const
{
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

float TensorData::at(std::span<std::size_t const> indices) const
{
    if (!_storage.isValid()) {
        throw std::runtime_error("TensorData::at: tensor has no storage");
    }
    return _storage.getValueAt(indices);
}

std::vector<float> TensorData::row(std::size_t index) const
{
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

std::span<float const> TensorData::flatData() const
{
    if (!_storage.isValid()) {
        throw std::runtime_error("TensorData::flatData: tensor has no storage");
    }
    return _storage.flatData();
}

std::vector<float> TensorData::materializeFlat() const
{
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

TensorData TensorData::materialize() const
{
    if (!_storage.isValid()) {
        return *this; // empty tensor
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

TensorData TensorData::toArmadillo() const
{
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

    // Materialize and convert
    return materialize();
}

arma::fmat const & TensorData::asArmadilloMatrix() const
{
    if (!_storage.isValid()) {
        throw std::logic_error("TensorData::asArmadilloMatrix: empty tensor");
    }
    auto const * arma_storage = _storage.tryGetAs<ArmadilloTensorStorage>();
    if (!arma_storage) {
        throw std::logic_error(
            "TensorData::asArmadilloMatrix: storage is not Armadillo-backed "
            "(use toArmadillo() first)");
    }
    return arma_storage->matrix(); // throws internally if not 2D
}

arma::fcube const & TensorData::asArmadilloCube() const
{
    if (!_storage.isValid()) {
        throw std::logic_error("TensorData::asArmadilloCube: empty tensor");
    }
    auto const * arma_storage = _storage.tryGetAs<ArmadilloTensorStorage>();
    if (!arma_storage) {
        throw std::logic_error(
            "TensorData::asArmadilloCube: storage is not Armadillo-backed "
            "(use toArmadillo() first)");
    }
    return arma_storage->cube(); // throws internally if not 3D
}

// =============================================================================
// Backend Conversion: LibTorch
// =============================================================================

#ifdef TENSOR_BACKEND_LIBTORCH
TensorData TensorData::toLibTorch() const
{
    // Already LibTorch-backed? Return shallow copy.
    if (_storage.isValid() &&
        _storage.getStorageType() == TensorStorageType::LibTorch) {
        return *this;
    }

    if (!_storage.isValid()) {
        throw std::logic_error("TensorData::toLibTorch: empty tensor");
    }

    // Materialize to row-major flat data, then wrap in torch::Tensor
    auto flat = materializeFlat();
    auto const s = _dimensions.shape();

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

torch::Tensor const & TensorData::asTorchTensor() const
{
    if (!_storage.isValid()) {
        throw std::logic_error("TensorData::asTorchTensor: empty tensor");
    }
    auto const * torch_storage = _storage.tryGetAs<LibTorchTensorStorage>();
    if (!torch_storage) {
        throw std::logic_error(
            "TensorData::asTorchTensor: storage is not LibTorch-backed "
            "(use toLibTorch() first)");
    }
    return torch_storage->tensor();
}
#endif // TENSOR_BACKEND_LIBTORCH

// =============================================================================
// Mutation
// =============================================================================

void TensorData::setData(std::vector<float> const & data,
                            std::vector<std::size_t> const & new_shape)
{
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
                            std::vector<std::size_t> const & new_shape)
{
    // For now, delegating to the const-ref version since makeStorage takes const ref.
    // The Dense storage constructor does accept by value, but we'd need to refactor
    // makeStorage to accept rvalue. Keeping this simple for initial implementation.
    setData(static_cast<std::vector<float> const &>(data), new_shape);
}

// =============================================================================
// Storage Access
// =============================================================================

TensorStorageWrapper const & TensorData::storage() const noexcept
{
    return _storage;
}

bool TensorData::isContiguous() const noexcept
{
    if (!_storage.isValid()) {
        return false;
    }
    return _storage.isContiguous();
}

bool TensorData::isEmpty() const noexcept
{
    return !_storage.isValid();
}

// =============================================================================
// TimeFrame
// =============================================================================

void TensorData::setTimeFrame(std::shared_ptr<TimeFrame> tf)
{
    _time_frame = std::move(tf);
}

std::shared_ptr<TimeFrame> TensorData::getTimeFrame() const noexcept
{
    return _time_frame;
}

// =============================================================================
// Column Names Mutation
// =============================================================================

void TensorData::setColumnNames(std::vector<std::string> names)
{
    _dimensions.setColumnNames(std::move(names));
}
