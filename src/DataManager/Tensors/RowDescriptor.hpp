#ifndef ROW_DESCRIPTOR_HPP
#define ROW_DESCRIPTOR_HPP

/**
 * @file RowDescriptor.hpp
 * @brief Describes the temporal meaning of a tensor's row axis
 *
 * A tensor's row axis (axis 0) can represent time-indexed samples
 * (via TimeIndexStorage), time intervals (e.g., trials), or plain ordinal
 * indices with no time semantics. RowDescriptor captures this as a
 * discriminated variant with factory constructors and type-specific accessors.
 *
 * Part of the TensorData refactor (step 1).
 * @see tensor_data_refactor_proposal.md §5 for design rationale.
 */

#include "TimeFrame/TimeFrameIndex.hpp"   // TimeFrameIndex
#include "TimeFrame/TimeIndexStorage.hpp" // TimeIndexStorage
#include "TimeFrame/interval_data.hpp"    // TimeFrameInterval

#include <cstddef>      // size_t
#include <memory>       // std::shared_ptr
#include <span>         // std::span
#include <variant>      // std::variant 
#include <vector>       // std::vector

class TimeFrame;

/**
 * @brief Describes what the "row axis" (typically axis 0) of a tensor represents
 *
 * A tensor's rows can represent:
 * - **TimeFrameIndex**: Each row corresponds to a single time point (indexed via TimeIndexStorage)
 * - **Interval**: Each row corresponds to a time interval (e.g., a trial)
 * - **Ordinal**: Rows have no temporal meaning — plain 0..N-1 indexing
 */
enum class RowType {
    TimeFrameIndex,  ///< Each row is a single TimeFrameIndex
    Interval,        ///< Each row is a TimeFrameInterval
    Ordinal          ///< Rows have no temporal meaning (plain 0..N-1)
};

/**
 * @brief Describes the row structure of a tensor
 *
 * This is a value-type that captures what the "rows" of a tensor mean.
 * It holds either ordinal row count, time index storage, or interval data.
 *
 * Consumers query `type()` and then access the appropriate data:
 * @code
 * switch (rows.type()) {
 *     case RowType::TimeFrameIndex:
 *         // Use rows.timeStorage() and rows.timeFrame()
 *         break;
 *     case RowType::Interval:
 *         // Use rows.intervals() and rows.timeFrame()
 *         break;
 *     case RowType::Ordinal:
 *         // Just use rows.count() for 0..N-1
 *         break;
 * }
 * @endcode
 */
class RowDescriptor {
public:
    /// Label type for a single row — used for display / export
    using RowLabel = std::variant<std::monostate, std::size_t, TimeFrameIndex, TimeFrameInterval>;

    // ========== Factory methods ==========

    /**
     * @brief Create an ordinal row descriptor (no temporal meaning)
     * @param count Number of rows
     */
    static RowDescriptor ordinal(std::size_t count);

    /**
     * @brief Create a time-indexed row descriptor
     * @param storage TimeIndexStorage mapping array positions to TimeFrameIndex values
     * @param time_frame The TimeFrame providing absolute time reference
     * @throws std::invalid_argument if storage or time_frame is null
     */
    static RowDescriptor fromTimeIndices(
        std::shared_ptr<TimeIndexStorage> storage,
        std::shared_ptr<TimeFrame> time_frame);

    /**
     * @brief Create an interval-based row descriptor
     * @param intervals Vector of TimeFrameInterval, one per row
     * @param time_frame The TimeFrame providing absolute time reference
     * @throws std::invalid_argument if time_frame is null
     */
    static RowDescriptor fromIntervals(
        std::vector<TimeFrameInterval> intervals,
        std::shared_ptr<TimeFrame> time_frame);

    // ========== Queries ==========

    /**
     * @brief Get the row type
     */
    [[nodiscard]] RowType type() const noexcept { return _type; }

    /**
     * @brief Number of rows
     */
    [[nodiscard]] std::size_t count() const noexcept;

    // ========== Type-specific access ==========

    /**
     * @brief Access the TimeIndexStorage (only valid for TimeFrameIndex rows)
     * @throws std::logic_error if type() != RowType::TimeFrameIndex
     */
    [[nodiscard]] TimeIndexStorage const & timeStorage() const;

    /**
     * @brief Access the shared TimeIndexStorage pointer
     * @throws std::logic_error if type() != RowType::TimeFrameIndex
     */
    [[nodiscard]] std::shared_ptr<TimeIndexStorage> timeStoragePtr() const;

    /**
     * @brief Access the intervals (only valid for Interval rows)
     * @throws std::logic_error if type() != RowType::Interval
     */
    [[nodiscard]] std::span<TimeFrameInterval const> intervals() const;

    /**
     * @brief Access the TimeFrame (nullptr for Ordinal rows)
     */
    [[nodiscard]] std::shared_ptr<TimeFrame> timeFrame() const noexcept;

    // ========== Row-level labeling ==========

    /**
     * @brief Get a label for a specific row
     *
     * Returns:
     * - `std::size_t` for Ordinal rows (the row index)
     * - `TimeFrameIndex` for TimeFrameIndex rows
     * - `TimeFrameInterval` for Interval rows
     *
     * @param row Row index (0-based)
     * @throws std::out_of_range if row >= count()
     */
    [[nodiscard]] RowLabel labelAt(std::size_t row) const;

    // ========== Comparison ==========

    bool operator==(RowDescriptor const & other) const;
    bool operator!=(RowDescriptor const & other) const { return !(*this == other); }

private:
    RowDescriptor() = default;

    RowType _type{RowType::Ordinal};
    std::size_t _ordinal_count{0};                         ///< For Ordinal rows
    std::shared_ptr<TimeIndexStorage> _time_storage;       ///< For TimeFrameIndex rows
    std::vector<TimeFrameInterval> _intervals;             ///< For Interval rows
    std::shared_ptr<TimeFrame> _time_frame;                ///< Shared time reference
};

#endif // ROW_DESCRIPTOR_HPP
