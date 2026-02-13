#ifndef TENSOR_DATA_V2_HPP
#define TENSOR_DATA_V2_HPP

/**
 * @file TensorData.hpp
 * @brief Refactored TensorData — unified N-dimensional tensor with named axes,
 *        multiple storage backends, view/lazy support, and DataTraits.
 *
 * This is step 5 of the TensorData refactor (see tensor_data_refactor_proposal.md).
 * It wires together:
 * - DimensionDescriptor (named axes, shape, column names)
 * - RowDescriptor (time-indexed, interval, or ordinal rows)
 * - TensorStorageWrapper (Sean Parent type-erasure over storage backends)
 * - ObserverData (change propagation)
 * - DataTypeTraitsBase (trait-based dispatch in the v2 transform system)
 *
 * ## Design Principles
 *
 * 1. **Float-only.** All elements are `float`.
 * 2. **Non-ragged.** All rows have the same number of columns.
 * 3. **Time is metadata.** The time axis lives in RowDescriptor + TimeFrame,
 *    not in the storage buffer.
 * 4. **Backend is a storage concern.** Armadillo (≤3D) and Dense (>3D) are
 *    always available. LibTorch is behind `#ifdef TENSOR_BACKEND_LIBTORCH`.
 * 5. **Views are immutable.** Mutation requires materialization.
 *
 * ## Migration Note
 *
 * This class coexists with the legacy Tensor_Data.hpp during the transition.
 * New code should use this class. Legacy call sites will be migrated in
 * subsequent steps.
 */

#include "DimensionDescriptor.hpp"
#include "RowDescriptor.hpp"
#include "storage/TensorStorageWrapper.hpp"

#include "Observer/Observer_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"
#include "DataManager/TypeTraits/DataTypeTraits.hpp"

#include <armadillo>

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#ifdef TENSOR_BACKEND_LIBTORCH
// torch's c10 logging defines a CHECK macro that conflicts with testing
// frameworks (Catch2) and other libraries. Save the current CHECK definition
// (if any), include torch, then restore it.
#pragma push_macro("CHECK")
#include <torch/torch.h>
#pragma pop_macro("CHECK")
#endif

// Forward declarations
class TimeIndexStorage;

/**
 * @brief Refactored N-dimensional tensor with named axes, multiple storage
 *        backends, and first-class DataManager integration.
 *
 * ## Quick Start
 *
 * @code
 * // 2D time-series matrix (e.g., spectrogram)
 * auto spec = TensorData::createTimeSeries2D(
 *     flat_magnitudes, num_time_bins, num_freq_bins,
 *     time_storage, time_frame,
 *     {"0-10 Hz", "10-20 Hz", "20-30 Hz"});
 *
 * auto band = spec.getColumn("10-20 Hz");
 *
 * // Direct Armadillo access for mlpack
 * arma::fmat const& m = spec.asArmadilloMatrix();
 * @endcode
 *
 * @see tensor_data_refactor_proposal.md for full design rationale
 */
class TensorData : public ObserverData {
public:
    // ========== DataTraits ==========

    /**
     * @brief Traits for generic trait-based dispatch in transforms v2
     */
    struct DataTraits : WhiskerToolbox::TypeTraits::DataTypeTraitsBase<TensorData, float> {
        static constexpr bool is_ragged = false;
        static constexpr bool is_temporal = true;
        static constexpr bool has_entity_ids = false;
        static constexpr bool is_spatial = false;
    };

    // ========== Construction ==========

    /**
     * @brief Default constructor — empty tensor (no storage, ordinal 0 rows)
     */
    TensorData();

    /**
     * @brief Destructor (non-trivial due to shared_ptr members)
     */
    ~TensorData();

    /// Copy
    TensorData(TensorData const & other);
    TensorData & operator=(TensorData const & other);

    /// Move
    TensorData(TensorData && other) noexcept;
    TensorData & operator=(TensorData && other) noexcept;

    // ========== Named Constructors (Factory Methods) ==========

    /**
     * @brief Create a 2D time-series tensor (rows = time, columns = features)
     *
     * Uses ArmadilloTensorStorage internally. Axes named "time" and "channel".
     *
     * @param data Flat row-major data (size must == num_rows * num_cols)
     * @param num_rows Number of rows (time points)
     * @param num_cols Number of columns (features / frequency bins / channels)
     * @param time_storage TimeIndexStorage mapping row indices to TimeFrameIndex
     * @param time_frame Shared TimeFrame for absolute time reference
     * @param column_names Optional column labels (size must match num_cols if provided)
     * @throws std::invalid_argument on size mismatches or null pointers
     */
    [[nodiscard]] static TensorData createTimeSeries2D(
        std::vector<float> const & data,
        std::size_t num_rows,
        std::size_t num_cols,
        std::shared_ptr<TimeIndexStorage> time_storage,
        std::shared_ptr<TimeFrame> time_frame,
        std::vector<std::string> column_names = {});

    /**
     * @brief Create a 2D tensor with interval-based rows
     *
     * Each row corresponds to an interval (e.g., trial). Uses ArmadilloTensorStorage.
     *
     * @param data Flat row-major data (size must == num_rows * num_cols)
     * @param num_rows Number of rows (intervals)
     * @param num_cols Number of columns
     * @param intervals One TimeFrameInterval per row
     * @param time_frame Shared TimeFrame for time reference
     * @param column_names Optional column labels
     * @throws std::invalid_argument on size mismatches
     */
    [[nodiscard]] static TensorData createFromIntervals(
        std::vector<float> const & data,
        std::size_t num_rows,
        std::size_t num_cols,
        std::vector<TimeFrameInterval> intervals,
        std::shared_ptr<TimeFrame> time_frame,
        std::vector<std::string> column_names = {});

    /**
     * @brief Create an N-dimensional tensor from flat data and axis descriptors
     *
     * Uses ArmadilloTensorStorage for ≤3D, DenseTensorStorage for >3D.
     * Row descriptor is Ordinal (no time semantics).
     *
     * @param data Flat row-major data
     * @param axes Ordered axis descriptors (outermost first)
     * @throws std::invalid_argument if data size doesn't match total elements
     */
    [[nodiscard]] static TensorData createND(
        std::vector<float> const & data,
        std::vector<AxisDescriptor> axes);

    /**
     * @brief Create a 2D tensor from an Armadillo matrix (zero-copy)
     *
     * Column names are optional. Row descriptor is Ordinal.
     *
     * @param matrix Armadillo float matrix (moved into storage)
     * @param column_names Optional column labels
     */
    [[nodiscard]] static TensorData createFromArmadillo(
        arma::fmat matrix,
        std::vector<std::string> column_names = {});

    /**
     * @brief Create a 3D tensor from an Armadillo cube (zero-copy)
     *
     * @param cube Armadillo float cube (moved into storage)
     * @param axes Optional axis descriptors; if empty, defaults to
     *        {"dim0", nslices}, {"dim1", nrows}, {"dim2", ncols}
     */
    [[nodiscard]] static TensorData createFromArmadillo(
        arma::fcube cube,
        std::vector<AxisDescriptor> axes = {});

    /**
     * @brief Create a 2D ordinal tensor from flat data
     *
     * Convenience for matrices without time semantics.
     * Uses ArmadilloTensorStorage.
     *
     * @param data Flat row-major data
     * @param num_rows Number of rows
     * @param num_cols Number of columns
     * @param column_names Optional column labels
     */
    [[nodiscard]] static TensorData createOrdinal2D(
        std::vector<float> const & data,
        std::size_t num_rows,
        std::size_t num_cols,
        std::vector<std::string> column_names = {});

#ifdef TENSOR_BACKEND_LIBTORCH
    /**
     * @brief Create from a LibTorch tensor
     *
     * @param tensor torch::Tensor (moved into LibTorchTensorStorage)
     * @param axes Optional axis descriptors; if empty, auto-generated
     */
    [[nodiscard]] static TensorData createFromTorch(
        torch::Tensor tensor,
        std::vector<AxisDescriptor> axes = {});
#endif

    // ========== Dimension Queries ==========

    /**
     * @brief Get the full dimension descriptor
     */
    [[nodiscard]] DimensionDescriptor const & dimensions() const noexcept;

    /**
     * @brief Number of dimensions (axes)
     */
    [[nodiscard]] std::size_t ndim() const noexcept;

    /**
     * @brief Shape as a vector of sizes (one per axis)
     */
    [[nodiscard]] std::vector<std::size_t> shape() const;

    // ========== Row Queries ==========

    /**
     * @brief Get the row descriptor
     */
    [[nodiscard]] RowDescriptor const & rows() const noexcept;

    /**
     * @brief Get the row type (TimeFrameIndex, Interval, or Ordinal)
     */
    [[nodiscard]] RowType rowType() const noexcept;

    /**
     * @brief Number of rows (axis 0 size, or RowDescriptor::count() for ordinal)
     */
    [[nodiscard]] std::size_t numRows() const noexcept;

    // ========== Column/Channel Access ==========

    /**
     * @brief Whether this tensor has named columns
     */
    [[nodiscard]] bool hasNamedColumns() const noexcept;

    /**
     * @brief Get the column names (empty if not set)
     */
    [[nodiscard]] std::vector<std::string> const & columnNames() const noexcept;

    /**
     * @brief Number of columns (last axis size for ≥2D, 1 for 1D, 0 for scalar)
     */
    [[nodiscard]] std::size_t numColumns() const noexcept;

    /**
     * @brief Get column data by index
     *
     * Returns a vector of float values for all rows at the given column.
     *
     * @param index Column index (0-based)
     * @throws std::out_of_range if index >= numColumns()
     */
    [[nodiscard]] std::vector<float> getColumn(std::size_t index) const;

    /**
     * @brief Get column data by name
     *
     * @param name Column name (must match one set via setColumnNames / factory)
     * @throws std::invalid_argument if name not found or no named columns
     */
    [[nodiscard]] std::vector<float> getColumn(std::string_view name) const;

    // ========== Element Access ==========

    /**
     * @brief Get a single element by multi-dimensional index
     *
     * @param indices One index per dimension
     * @throws std::out_of_range / std::invalid_argument on index errors
     */
    [[nodiscard]] float at(std::span<std::size_t const> indices) const;

    /**
     * @brief Get an entire row as a flat vector (all columns for that row)
     *
     * For a 2D tensor with shape [R, C], returns C floats.
     * For a 3D tensor with shape [D0, D1, D2], returns D1*D2 floats.
     *
     * @param index Row index (axis 0)
     * @throws std::out_of_range if index >= axis(0).size
     */
    [[nodiscard]] std::vector<float> row(std::size_t index) const;

    /**
     * @brief Fast-path flat data access (contiguous storage only)
     *
     * @throws std::runtime_error if storage is not contiguous
     * @note Layout depends on the backend — Armadillo is column-major,
     *       Dense is row-major. For guaranteed row-major, use materializeFlat().
     */
    [[nodiscard]] std::span<float const> flatData() const;

    /**
     * @brief Materialize all data as a row-major flat vector (always works)
     *
     * If storage is already contiguous and row-major, returns a copy of flatData().
     * Otherwise, reconstructs row-major order from element access.
     */
    [[nodiscard]] std::vector<float> materializeFlat() const;

    // ========== Backend Conversion ==========

    /**
     * @brief Materialize the tensor into owned storage
     *
     * If the tensor is already backed by owned storage (Armadillo or Dense),
     * returns a copy. If backed by a View or Lazy storage, computes and
     * returns a new TensorData with materialized data.
     *
     * Uses ArmadilloTensorStorage for ≤3D, DenseTensorStorage for >3D.
     */
    [[nodiscard]] TensorData materialize() const;

    /**
     * @brief Convert to Armadillo-backed tensor (must be ≤3D)
     *
     * If already Armadillo-backed, returns a shallow copy (shared storage).
     * Otherwise, materializes into a new ArmadilloTensorStorage.
     *
     * @throws std::logic_error if ndim() > 3
     */
    [[nodiscard]] TensorData toArmadillo() const;

    /**
     * @brief Direct access to the underlying Armadillo matrix (2D only)
     *
     * @throws std::logic_error if not 2D or not Armadillo-backed
     */
    [[nodiscard]] arma::fmat const & asArmadilloMatrix() const;

    /**
     * @brief Direct access to the underlying Armadillo cube (3D only)
     *
     * @throws std::logic_error if not 3D or not Armadillo-backed
     */
    [[nodiscard]] arma::fcube const & asArmadilloCube() const;

#ifdef TENSOR_BACKEND_LIBTORCH
    /**
     * @brief Convert to LibTorch-backed tensor
     *
     * If already LibTorch-backed, returns a shallow copy.
     * Otherwise, materializes flat data and wraps in torch::Tensor.
     */
    [[nodiscard]] TensorData toLibTorch() const;

    /**
     * @brief Direct access to the underlying torch::Tensor
     *
     * @throws std::logic_error if not LibTorch-backed
     */
    [[nodiscard]] torch::Tensor const & asTorchTensor() const;
#endif

    // ========== Mutation ==========

    /**
     * @brief Replace all data with new flat data and shape
     *
     * Creates new owned storage (Armadillo ≤3D, Dense >3D).
     * Notifies observers.
     *
     * @param data New flat row-major data
     * @param new_shape New shape
     * @throws std::invalid_argument on size mismatch
     */
    void setData(std::vector<float> const & data,
                 std::vector<std::size_t> const & new_shape);

    /**
     * @brief Replace all data with new flat data and shape (move overload)
     */
    void setData(std::vector<float> && data,
                 std::vector<std::size_t> const & new_shape);

    // ========== Storage Access ==========

    /**
     * @brief Get the underlying storage wrapper (for advanced backend access)
     */
    [[nodiscard]] TensorStorageWrapper const & storage() const noexcept;

    /**
     * @brief Whether the underlying storage is contiguous in memory
     */
    [[nodiscard]] bool isContiguous() const noexcept;

    /**
     * @brief Whether this tensor has valid (non-empty) storage
     */
    [[nodiscard]] bool isEmpty() const noexcept;

    // ========== TimeFrame ==========

    /**
     * @brief Set the time frame for this tensor
     *
     * This is set automatically by factory methods that take a TimeFrame.
     * Can also be set manually for ordinal tensors that later gain time semantics.
     */
    void setTimeFrame(std::shared_ptr<TimeFrame> tf);

    /**
     * @brief Get the time frame (may be nullptr for ordinal tensors)
     */
    [[nodiscard]] std::shared_ptr<TimeFrame> getTimeFrame() const noexcept;

    // ========== Column Names Mutation ==========

    /**
     * @brief Set or replace column names
     *
     * @param names Column names; must match the last axis size
     * @throws std::invalid_argument on size mismatch
     */
    void setColumnNames(std::vector<std::string> names);

private:
    // ========== Private Construction Helpers ==========

    /**
     * @brief Fully specified internal constructor
     */
    TensorData(DimensionDescriptor dimensions,
                 RowDescriptor rows,
                 TensorStorageWrapper storage,
                 std::shared_ptr<TimeFrame> time_frame);

    // ========== Members ==========
    DimensionDescriptor _dimensions;          ///< Named axes + shape
    RowDescriptor _rows;                      ///< Row type (time/interval/ordinal)
    TensorStorageWrapper _storage;            ///< Type-erased storage backend
    std::shared_ptr<TimeFrame> _time_frame;   ///< Absolute time reference (nullable)
};



#endif // TENSOR_DATA_V2_HPP
