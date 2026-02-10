#ifndef DIMENSION_DESCRIPTOR_HPP
#define DIMENSION_DESCRIPTOR_HPP

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief Describes a single axis of a tensor
 *
 * Each axis has a human-readable name (e.g., "time", "channel", "frequency")
 * and a fixed size. Axes are identified by string names — no enum taxonomy
 * is imposed. Consumers search by conventional names.
 */
struct AxisDescriptor {
    std::string name;   ///< e.g., "time", "frequency", "channel"
    std::size_t size;   ///< Length along this axis

    bool operator==(AxisDescriptor const & other) const {
        return name == other.name && size == other.size;
    }
    bool operator!=(AxisDescriptor const & other) const { return !(*this == other); }
};

/**
 * @brief Describes the shape and named axes of a tensor
 *
 * A tensor has an ordered list of named axes. Each axis has a name and a size.
 * The descriptor also supports optional named columns on one designated axis,
 * useful for tabular / feature-matrix representations.
 *
 * Axis names are freeform strings. Conventional names include:
 * - "time"      — row axis indexed by TimeIndexStorage
 * - "channel"   — named columns / features
 * - "frequency" — frequency bins (spectrograms)
 * - "batch"     — batch dimension (model I/O)
 * - "height", "width" — spatial (image-like tensors)
 *
 * Row-major strides are computed on construction for efficient flat-index
 * calculations.
 */
class DimensionDescriptor {
public:
    // ========== Construction ==========

    /**
     * @brief Construct from a list of axis descriptors
     * @param axes Ordered list of axes (outermost first, innermost last)
     * @throws std::invalid_argument if any axis has size == 0 or duplicate names exist
     */
    explicit DimensionDescriptor(std::vector<AxisDescriptor> axes);

    /**
     * @brief Default constructor — zero-dimensional (scalar) tensor
     */
    DimensionDescriptor() = default;

    // ========== Queries ==========

    /**
     * @brief Number of dimensions (axes)
     */
    [[nodiscard]] std::size_t ndim() const noexcept { return _axes.size(); }

    /**
     * @brief Total number of elements (product of all axis sizes)
     *
     * Returns 1 for a scalar (zero axes). Returns 0 only if an axis has size 0,
     * which is disallowed by the constructor.
     */
    [[nodiscard]] std::size_t totalElements() const noexcept;

    /**
     * @brief Get the shape as a vector of sizes (one per axis)
     */
    [[nodiscard]] std::vector<std::size_t> shape() const;

    /**
     * @brief Access an axis descriptor by positional index
     * @throws std::out_of_range if i >= ndim()
     */
    [[nodiscard]] AxisDescriptor const & axis(std::size_t i) const;

    /**
     * @brief Find the positional index of an axis by name
     * @return The index, or std::nullopt if no axis has that name
     */
    [[nodiscard]] std::optional<std::size_t> findAxis(std::string_view name) const noexcept;

    /**
     * @brief Row-major strides (precomputed)
     *
     * stride[i] = product of sizes of axes i+1 .. ndim-1.
     * For a scalar tensor, returns an empty vector.
     */
    [[nodiscard]] std::vector<std::size_t> const & strides() const noexcept { return _strides; }

    /**
     * @brief Compute the flat (row-major) index for a set of per-axis indices
     * @param indices One index per axis
     * @return The flat offset into a row-major buffer
     * @throws std::invalid_argument if indices.size() != ndim()
     * @throws std::out_of_range if any index is out of bounds
     */
    [[nodiscard]] std::size_t flatIndex(std::vector<std::size_t> const & indices) const;

    // ========== Dimensionality predicates ==========

    [[nodiscard]] bool is1D() const noexcept { return ndim() == 1; }
    [[nodiscard]] bool is2D() const noexcept { return ndim() == 2; }
    [[nodiscard]] bool is3D() const noexcept { return ndim() == 3; }
    [[nodiscard]] bool isAtLeast(std::size_t n) const noexcept { return ndim() >= n; }

    // ========== Named column support ==========

    /**
     * @brief Assign human-readable names to columns (last axis)
     *
     * @param names Column names; must have size == last axis size
     * @throws std::invalid_argument if names.size() doesn't match last axis,
     *         or if input is provided on a scalar tensor
     */
    void setColumnNames(std::vector<std::string> names);

    /**
     * @brief Get the column names (empty if not set)
     */
    [[nodiscard]] std::vector<std::string> const & columnNames() const noexcept { return _column_names; }

    /**
     * @brief Whether column names have been assigned
     */
    [[nodiscard]] bool hasColumnNames() const noexcept { return !_column_names.empty(); }

    /**
     * @brief Find a column index by name
     * @return The column index, or std::nullopt if not found (or no column names set)
     */
    [[nodiscard]] std::optional<std::size_t> findColumn(std::string_view name) const noexcept;

    // ========== Comparison ==========

    bool operator==(DimensionDescriptor const & other) const;
    bool operator!=(DimensionDescriptor const & other) const { return !(*this == other); }

private:
    std::vector<AxisDescriptor> _axes;
    std::vector<std::size_t> _strides;         ///< Precomputed row-major strides
    std::vector<std::string> _column_names;    ///< Optional, size == last axis size

    void computeStrides();
    void validateAxes() const;
};

#endif // DIMENSION_DESCRIPTOR_HPP
