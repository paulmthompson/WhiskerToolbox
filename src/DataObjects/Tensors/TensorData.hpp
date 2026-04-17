#ifndef TENSOR_DATA_V2_HPP
#define TENSOR_DATA_V2_HPP

/**
 * @file TensorData.hpp
 * @brief TensorData — unified N-dimensional tensor with named axes,
 *        multiple storage backends, view/lazy support, and DataTraits.
 
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
 */

#include "DimensionDescriptor.hpp"
#include "RowDescriptor.hpp"
#include "storage/LazyColumnTensorStorage.hpp"
#include "storage/TensorStorageWrapper.hpp"

#include "Observer/Observer_Data.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"
#include "TimeFrame/interval_data.hpp"
#include "TypeTraits/DataTypeTraits.hpp"

#include <armadillo>

#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class TimeIndexStorage;
class LazyColumnTensorStorage;
class TensorData;
class TimeFrame;

/**
 * @brief Callback that wires observer invalidation after lazy tensor construction.
 *
 * Receives the lazy storage (for invalidateColumn) and the TensorData
 * (for notifyObservers). Called exactly once, at the end of createFromLazyColumns().
 * Constructed at the builder layer (e.g., TensorColumnBuilders.hpp in TransformsV2).
 *
 */
using InvalidationWiringFn = std::function<void(LazyColumnTensorStorage &, TensorData &)>;

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
     * @param time_frame Shared TimeFrame for absolute time reference (may be nullptr;
     *        DataManager assigns the correct TimeFrame when registered)
     * @param column_names Optional column labels (size must match num_cols if provided)
     *
     * @pre time_storage != nullptr (enforcement: exception)
     * @pre time_storage->size() == num_rows (enforcement: exception)
     * @pre data.size() == num_rows * num_cols (enforcement: exception)
     * @pre column_names.empty() || column_names.size() == num_cols (enforcement: exception)
     * @pre num_rows * num_cols does not overflow std::size_t (enforcement: none) [LOW]
     *
     * @throws std::invalid_argument on size mismatches or null time_storage
     */
    [[nodiscard]] static TensorData createTimeSeries2D(
            std::vector<float> const & data,
            std::size_t num_rows,
            std::size_t num_cols,
            std::shared_ptr<TimeIndexStorage> time_storage,
            std::shared_ptr<TimeFrame> time_frame,
            std::vector<std::string> column_names = {});

    /**
     * @brief Create a 2D time-series tensor from a pre-built Armadillo matrix
     *
     * Zero-copy variant: the matrix is moved directly into ArmadilloTensorStorage,
     * avoiding row-major → column-major conversion overhead.
     * Axes named "time" and "channel".
     *
     * @param matrix Armadillo float matrix (moved into storage)
     * @param time_storage TimeIndexStorage mapping row indices to TimeFrameIndex
     * @param time_frame Shared TimeFrame for absolute time reference (may be nullptr)
     * @param column_names Optional column labels
     *
     * @pre time_storage != nullptr (enforcement: exception)
     * @pre time_storage->size() == matrix.n_rows (enforcement: exception)
     * @pre matrix.n_rows > 0 && matrix.n_cols > 0 (enforcement: exception)
     * @pre column_names.empty() || column_names.size() == matrix.n_cols (enforcement: exception)
     */
    [[nodiscard]] static TensorData createTimeSeries2D(
            arma::fmat matrix,
            std::shared_ptr<TimeIndexStorage> time_storage,
            std::shared_ptr<TimeFrame> time_frame,
            std::vector<std::string> column_names = {});

    /**
     * @brief Create a 2D time-series tensor from a custom storage backend.
     *
     * Enables non-Armadillo backends (e.g., MmapTensorStorage) to be used
     * as TensorData with time-indexed rows.
     *
     * @param storage Type-erased storage wrapper (must report 2D shape).
     * @param time_storage Dense time index storage (size must match storage rows).
     * @param time_frame Shared TimeFrame for time reference (may be nullptr;
     *        DataManager assigns the correct TimeFrame when registered).
     * @param column_names Optional column labels.
     *
     * @pre time_storage != nullptr (enforcement: exception)
     * @pre time_storage->size() == storage.shape()[0] (enforcement: exception)
     * @pre storage.shape().size() == 2 (enforcement: exception)
     */
    [[nodiscard]] static TensorData createTimeSeries2DFromStorage(
            TensorStorageWrapper storage,
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
     * @param time_frame Shared TimeFrame for time reference (may be nullptr;
     *        DataManager assigns the correct TimeFrame when registered)
     * @param column_names Optional column labels
     *
     * @pre intervals.size() == num_rows (enforcement: exception)
     * @pre data.size() == num_rows * num_cols (enforcement: exception)
     * @pre column_names.empty() || column_names.size() == num_cols (enforcement: exception)
     * @pre num_rows * num_cols does not overflow std::size_t (enforcement: none) [LOW]
     *
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
     *
     * @pre !axes.empty() (enforcement: exception)
     * @pre data.size() == product of all ax.size in axes (enforcement: exception)
     * @pre product of all ax.size does not overflow std::size_t (enforcement: none) [LOW]
     *
     * @throws std::invalid_argument if data size doesn't match total elements
     */
    [[nodiscard]] static TensorData createND(
            std::vector<float> const & data,
            std::vector<AxisDescriptor> const & axes);

    /**
     * @brief Create a 2D tensor from an Armadillo matrix (zero-copy)
     *
     * Column names are optional. Row descriptor is Ordinal.
     *
     * @param matrix Armadillo float matrix (moved into storage)
     * @param column_names Optional column labels
     *
     * @pre matrix.n_rows > 0 && matrix.n_cols > 0 (enforcement: exception)
     * @pre column_names.empty() || column_names.size() == matrix.n_cols (enforcement: exception)
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
     *
     * @pre cube.n_slices > 0 && cube.n_rows > 0 && cube.n_cols > 0 (enforcement: exception)
     * @pre axes.empty() || axes.size() == 3 (enforcement: none) [IMPORTANT]
     * @pre If !axes.empty(): axes[0].size == cube.n_slices, axes[1].size == cube.n_rows,
     *      axes[2].size == cube.n_cols — mismatched sizes will make dims.shape() lie
     *      silently (enforcement: none) [IMPORTANT]
     * @pre No zero-size axes and no duplicate axis names in provided axes
     *      (enforcement: exception)
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

    /**
     * @brief Create a 2D tensor with lazily-computed columns
     *
     * Each column is backed by a type-erased provider function that computes
     * the column data on demand. Results are cached per-column and can be
     * invalidated to trigger recomputation.
     *
     * Column names are taken from the ColumnSource::name fields.
     * Row semantics are specified by the RowDescriptor.
     *
     * @param num_rows Number of rows (each provider must return this many floats)
     * @param columns Column sources (name + provider); must not be empty
     * @param rows Row descriptor (ordinal, time-indexed, or interval)
     * @param wiring Optional callback for observer invalidation wiring;
     *               called once after construction with references to the
     *               LazyColumnTensorStorage and TensorData
     * @throws std::invalid_argument if num_rows == 0 or columns is empty
     */
    [[nodiscard]] static TensorData createFromLazyColumns(
            std::size_t num_rows,
            std::vector<ColumnSource> columns,
            RowDescriptor rows,
            InvalidationWiringFn const & wiring = {});

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

#endif// TENSOR_BACKEND_LIBTORCH


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

    // ========== Column Mutation (LazyColumnTensorStorage only) ==========

    /**
     * @brief Append a new lazy column
     *
     * Adds a column to the end of the tensor. Only valid when the underlying
     * storage is `LazyColumnTensorStorage`. Updates the dimension descriptor
     * (axis size + column names) and notifies observers.
     *
     * @param name Column name
     * @param provider Provider function that returns `numRows()` floats
     * @return The index of the newly appended column
     * @throws std::logic_error if the storage is not LazyColumnTensorStorage
     * @throws std::invalid_argument if provider is null
     */
    std::size_t appendColumn(std::string name, ColumnProviderFn provider);

    /**
     * @brief Remove a column by index
     *
     * Removes the column at the given index. Only valid when the underlying
     * storage is `LazyColumnTensorStorage`. The tensor must have more than
     * one column. Updates the dimension descriptor and notifies observers.
     *
     * @param col Column index to remove
     * @throws std::logic_error if the storage is not LazyColumnTensorStorage
     * @throws std::out_of_range if col >= numColumns()
     * @throws std::logic_error if removing the column would leave zero columns
     */
    void removeColumn(std::size_t col);

    // ========== Row Mutation ==========

    /**
     * @brief Append a row to the end of the tensor
     *
     * Works for 2D tensors backed by ArmadilloTensorStorage or DenseTensorStorage
     * with Ordinal or Interval row semantics.
     *
     * For Ordinal rows, the ordinal count is incremented.
     * For Interval rows, the provided interval is appended to the row descriptor.
     * TimeFrameIndex rows are not supported (time index storage is immutable).
     *
     * Updates the dimension descriptor (axis 0 size) and notifies observers.
     *
     * @param row_data Values for the new row (size must equal numColumns())
     * @throws std::logic_error if the tensor is not 2D, is backed by an
     *         unsupported storage type, or has TimeFrameIndex rows
     * @throws std::invalid_argument if row_data.size() != numColumns()
     */
    void appendRow(std::span<float const> row_data);

    /**
     * @brief Append a row with an associated interval
     *
     * Convenience overload for tensors with Interval row semantics.
     *
     * @param row_data Values for the new row
     * @param interval Time interval for the new row
     * @throws std::logic_error if rowType() != RowType::Interval, or
     *         storage is unsupported
     * @throws std::invalid_argument if row_data.size() != numColumns()
     */
    void appendRow(std::span<float const> row_data, TimeFrameInterval interval);

    /**
     * @brief Insert a row at a specific position
     *
     * Same storage and row-type constraints as appendRow(). Existing rows
     * at and after the insertion point are shifted down.
     *
     * @param index Position to insert before (0-based; numRows() appends)
     * @param row_data Values for the new row
     * @throws std::logic_error if unsupported storage or TimeFrameIndex rows
     * @throws std::out_of_range if index > numRows()
     * @throws std::invalid_argument if row_data.size() != numColumns()
     */
    void insertRow(std::size_t index, std::span<float const> row_data);

    /**
     * @brief Insert a row with an associated interval
     *
     * Convenience overload for tensors with Interval row semantics.
     *
     * @param index Position to insert before
     * @param row_data Values for the new row
     * @param interval Time interval for the new row
     * @throws std::logic_error if rowType() != RowType::Interval, or
     *         storage is unsupported
     * @throws std::out_of_range if index > numRows()
     * @throws std::invalid_argument if row_data.size() != numColumns()
     */
    void insertRow(std::size_t index, std::span<float const> row_data,
                   TimeFrameInterval interval);

    /**
     * @brief Overwrite an existing row's data in-place
     *
     * Works for 2D tensors backed by ArmadilloTensorStorage or DenseTensorStorage.
     * The row must already exist. Row descriptor and dimension metadata are unchanged.
     * Notifies observers.
     *
     * @param index Row index to overwrite (0-based)
     * @param row_data New values for the row (size must equal numColumns())
     * @throws std::logic_error if the tensor is not 2D or has unsupported storage
     * @throws std::out_of_range if index >= numRows()
     * @throws std::invalid_argument if row_data.size() != numColumns()
     */
    void setRow(std::size_t index, std::span<float const> row_data);

    /**
     * @brief Upsert rows keyed by TimeFrameIndex
     *
     * Merges the given frame→row-data pairs into this tensor. For each pair:
     * - If a row for that frame already exists, its data is overwritten.
     * - If no row exists for that frame, a new row is inserted in sorted order.
     *
     * After the operation, the tensor's rows are sorted by TimeFrameIndex and
     * backed by a fresh TimeIndexStorage. The tensor must be 2D and backed by
     * ArmadilloTensorStorage or DenseTensorStorage. If the tensor is empty
     * (no storage), it is initialized from the provided rows.
     *
     * @param frame_rows Pairs of (frame_index, row_data); row_data sizes must
     *        all equal numColumns() (or each other for empty tensors)
     * @param time_frame Shared TimeFrame for the resulting time-indexed rows
     * @pre All row_data vectors must have the same size
     * @throws std::invalid_argument if frame_rows is empty, row sizes are
     *         inconsistent, or time_frame is null
     * @throws std::logic_error if the tensor is non-empty and not 2D, or has
     *         unsupported storage
     */
    void upsertRows(std::vector<std::pair<int, std::vector<float>>> const & frame_rows,
                    std::shared_ptr<TimeFrame> time_frame);

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
    DimensionDescriptor _dimensions;       ///< Named axes + shape
    RowDescriptor _rows;                   ///< Row type (time/interval/ordinal)
    TensorStorageWrapper _storage;         ///< Type-erased storage backend
    std::shared_ptr<TimeFrame> _time_frame;///< Absolute time reference (nullable)
};


#endif// TENSOR_DATA_V2_HPP
