#ifndef LAZY_COLUMN_TENSOR_STORAGE_HPP
#define LAZY_COLUMN_TENSOR_STORAGE_HPP

/**
 * @file LazyColumnTensorStorage.hpp
 * @brief Lazy column-based tensor storage with type-erased column providers
 *
 * Each column is either a materialized std::vector<float> or a type-erased
 * provider function (std::function<std::vector<float>()>) that computes
 * the column on demand. Results are cached per-column and can be selectively
 * invalidated to trigger recomputation.
 *
 * This storage lives in the DataManager library and has **no dependency** on
 * TransformsV2, GatherResult, or DataManager itself. Provider functions are
 * assembled at higher architectural layers and injected as closures.
 *
 * Part of the TensorData refactor (step 7).
 * @see tensor_data_refactor_proposal.md §6.6 for design rationale.
 */

#include "TensorStorageBase.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

/**
 * @brief A function that produces an entire column of float values.
 *
 * Called once per materialization; result is cached. When the cache is
 * invalidated, the next access re-calls the provider. The returned vector
 * must have exactly `num_rows` elements.
 */
using ColumnProviderFn = std::function<std::vector<float>()>;

/**
 * @brief Describes one column source: a name and a provider function.
 *
 * The provider is called lazily on first access (or after invalidation).
 * The result is cached until explicitly invalidated.
 */
struct ColumnSource {
    std::string name;                                    ///< Column name
    ColumnProviderFn provider;                           ///< Produces num_rows floats
    mutable std::optional<std::vector<float>> cache;     ///< Populated on first access
};

/**
 * @brief Lazy column-based tensor storage backend
 *
 * Implements the TensorStorageBase CRTP interface for a 2D tensor where
 * each column is independently computed via a type-erased provider function.
 * Columns are materialized lazily (on first access) and cached.
 *
 * ## Key properties
 *
 * - **Shape** is always 2D: [num_rows, num_columns]
 * - **Not contiguous**: `flatData()` throws (no single contiguous buffer).
 *   Use `materializeAll()` + `flatCache()` or `getColumn()` for access.
 * - **Per-column caching**: each column is materialized independently.
 * - **Per-column invalidation**: `invalidateColumn(col)` clears one cache;
 *   `invalidateAll()` clears all.
 * - **Thread safety**: column materialization is guarded by a mutex so that
 *   concurrent reads don't race on cache population. The mutex is per-storage,
 *   not per-column, for simplicity.
 *
 * ## Relationship to other storages
 *
 * - `ArmadilloTensorStorage` / `DenseTensorStorage`: owned, contiguous data.
 * - `ViewTensorStorage`: zero-copy slice of another storage.
 * - `LazyColumnTensorStorage`: each column computed on demand, cached.
 *
 * Mixed materialized + lazy columns: a "materialized" column is just a
 * provider that returns fixed data (called once, cached forever if never
 * invalidated).
 */
class LazyColumnTensorStorage : public TensorStorageBase<LazyColumnTensorStorage> {
public:
    // ========== Construction ==========

    /**
     * @brief Construct from a row count and column sources
     *
     * @param num_rows Number of rows (each provider must return this many floats)
     * @param columns Column sources (name + provider + cache)
     * @throws std::invalid_argument if num_rows == 0 or columns is empty
     */
    LazyColumnTensorStorage(std::size_t num_rows, std::vector<ColumnSource> columns);

    // ========== Column-Level Control ==========

    /**
     * @brief Materialize a single column (populate its cache)
     *
     * No-op if already cached. Calls the column's provider function and
     * validates the result size.
     *
     * @param col Column index
     * @throws std::out_of_range if col >= numColumns()
     * @throws std::runtime_error if provider returns wrong size
     */
    void materializeColumn(std::size_t col) const;

    /**
     * @brief Materialize all columns
     *
     * Calls materializeColumn for each column that isn't already cached.
     */
    void materializeAll() const;

    /**
     * @brief Invalidate a single column's cache
     *
     * Clears the column's cached data and the flat cache. The next access
     * to this column will re-call the provider.
     *
     * @param col Column index
     * @throws std::out_of_range if col >= numColumns()
     */
    void invalidateColumn(std::size_t col);

    /**
     * @brief Invalidate all columns' caches
     *
     * Clears all cached data. The next access to any column will re-call
     * its provider.
     */
    void invalidateAll();

    /**
     * @brief Replace a column's provider (and invalidate its cache)
     *
     * @param col Column index
     * @param name New column name
     * @param provider New provider function
     * @throws std::out_of_range if col >= numColumns()
     */
    void setColumnProvider(std::size_t col, std::string name, ColumnProviderFn provider);

    /**
     * @brief Append a new column
     *
     * Adds a column to the end of the column list. The provider must return
     * exactly `numRows()` elements when called.
     *
     * @param name Column name
     * @param provider Provider function for the new column
     * @return The index of the newly appended column
     * @throws std::invalid_argument if provider is null
     */
    std::size_t appendColumn(std::string name, ColumnProviderFn provider);

    /**
     * @brief Remove a column by index
     *
     * Removes the column at the given index, shifting subsequent columns.
     * The storage must have more than one column (cannot remove the last column).
     *
     * @param col Column index to remove
     * @throws std::out_of_range if col >= numColumns()
     * @throws std::logic_error if removing the column would leave zero columns
     */
    void removeColumn(std::size_t col);

    /**
     * @brief Number of columns
     */
    [[nodiscard]] std::size_t numColumns() const noexcept;

    /**
     * @brief Number of rows
     */
    [[nodiscard]] std::size_t numRows() const noexcept;

    /**
     * @brief Get column names
     */
    [[nodiscard]] std::vector<std::string> columnNames() const;

    /**
     * @brief Get a flat row-major copy of all data (materializes everything)
     *
     * Unlike flatData() (which requires contiguous storage), this always works
     * by materializing all columns and assembling row-major output.
     *
     * @return Row-major flat vector of size num_rows * num_columns
     */
    [[nodiscard]] std::vector<float> materializeFlat() const;

    // ========== CRTP Implementation ==========

    [[nodiscard]] float getValueAtImpl(std::span<std::size_t const> indices) const;
    [[nodiscard]] std::span<float const> flatDataImpl() const;
    [[nodiscard]] std::vector<float> sliceAlongAxisImpl(std::size_t axis, std::size_t index) const;
    [[nodiscard]] std::vector<float> getColumnImpl(std::size_t col) const;
    [[nodiscard]] std::vector<std::size_t> shapeImpl() const;
    [[nodiscard]] std::size_t totalElementsImpl() const;
    [[nodiscard]] bool isContiguousImpl() const noexcept { return false; }
    [[nodiscard]] TensorStorageType getStorageTypeImpl() const noexcept {
        return TensorStorageType::Lazy;
    }
    [[nodiscard]] TensorStorageCache tryGetCacheImpl() const;

private:
    std::size_t _num_rows;
    mutable std::vector<ColumnSource> _columns;
    mutable std::unique_ptr<std::mutex> _mutex;  ///< Guards cache population (heap-allocated for movability)

    /**
     * @brief Ensure a column is materialized (caller must hold _mutex or
     *        call from a method that does)
     */
    void ensureMaterialized(std::size_t col) const;

    /**
     * @brief Validate column index
     * @throws std::out_of_range if col >= numColumns()
     */
    void validateColumn(std::size_t col) const;
};

#endif // LAZY_COLUMN_TENSOR_STORAGE_HPP
