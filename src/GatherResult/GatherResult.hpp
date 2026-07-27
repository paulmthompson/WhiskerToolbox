#ifndef GATHER_RESULT_HPP
#define GATHER_RESULT_HPP

/**
 * @file GatherResult.hpp
 * @brief Container for collected views created by gather operations
 *
 * This file provides the GatherResult template class and gather() function for
 * creating collections of view-backed data types from interval alignments.
 *
 * ## Overview
 *
 * GatherResult is designed for operations like raster plots, trial-aligned analysis,
 * and other scenarios where you need to create many views of a source data type
 * based on alignment intervals. Unlike registering each view in DataManager,
 * GatherResult keeps the collection self-contained.
 *
 * ## Key Features
 *
 * - **Zero-copy views**: Each element is a view into the source data
 * - **Range interface**: Standard begin()/end() for range-based for loops
 * - **Transform helper**: Apply functions to all views with transform()
 * - **Source tracking**: Access to source data and alignment intervals
 * - **Self-contained**: Does not pollute DataManager's registry
 *
 * ## Supported Types
 *
 * Any type T that has a static createView() method with signature:
 * @code
 * static std::shared_ptr<T> createView(
 *     std::shared_ptr<T const> source,
 *     TimeFrameIndex start,
 *     TimeFrameIndex end);
 * @endcode
 *
 * This includes:
 * - AnalogTimeSeries
 * - DigitalEventSeries
 * - DigitalIntervalSeries
 *
 * For RaggedTimeSeries-based types (MaskData, LineData, PointData), use
 * createTimeRangeCopy() which creates owning copies rather than views.
 *
 * ## Example Usage
 *
 * @code
 * // Create raster plot data from spike times aligned to trial intervals
 * auto spikes = dm->getData<DigitalEventSeries>("spikes");
 * auto trials = dm->getData<DigitalIntervalSeries>("trials");
 *
 * auto raster = gather(spikes, trials);
 *
 * // Iterate over trial-aligned spike views
 * for (auto const& trial_spikes : raster) {
 *     for (auto event : trial_spikes->view()) {
 *         // Each event retains its EntityId from the source
 *     }
 * }
 *
 * // Apply analysis to all trials
 * auto spike_counts = raster.transform([](auto const& trial) {
 *     return trial->size();
 * });
 *
 * // Get the interval for a specific trial
 * Interval trial_3_interval = raster.intervalAt(3);
 * @endcode
 *
 * @see DigitalIntervalSeries for alignment interval storage
 * @see DigitalEventSeries::createView() for event series views
 * @see AnalogTimeSeries::createView() for analog series views
 */

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"

#include <algorithm>
#include <cassert>
#include <concepts>
#include <functional>
#include <iterator>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <utility>
#include <vector>

// =============================================================================
// Concepts for View Creation
// =============================================================================

namespace Neuralyzer::Gather {

/**
 * @brief Concept for types that support zero-copy view creation
 *
 * Requires a static createView() method that takes a source shared_ptr
 * and start/end TimeFrameIndex values.
 */
template<typename T>
concept ViewableDataType = requires(
        std::shared_ptr<T> source,
        TimeFrameIndex start,
        TimeFrameIndex end) {
    { T::createView(source, start, end) } -> std::same_as<std::shared_ptr<T>>;
};

/**
 * @brief Concept for types that support int64_t-based view creation
 * 
 * Some types (like DigitalIntervalSeries) use int64_t for their createView signature.
 */
template<typename T>
concept ViewableDataTypeInt64 = requires(
        std::shared_ptr<T> source,
        int64_t start,
        int64_t end) {
    { T::createView(source, start, end) } -> std::same_as<std::shared_ptr<T>>;
};

/**
 * @brief Concept for types that support time range copy creation
 *
 * RaggedTimeSeries-based types use createTimeRangeCopy() which creates
 * an owning copy rather than a view.
 */
template<typename T>
concept CopyableTimeRangeDataType = requires(
        T & source,
        TimeFrameIndex start,
        TimeFrameIndex end) {
    { source.createTimeRangeCopy(start, end) } -> std::same_as<T>;
};

/**
 * @brief Concept for types that define an element_type alias
 *
 * Data containers that expose their element type (e.g., EventWithId for DigitalEventSeries)
 * satisfy this concept. This enables type-safe pipeline binding.
 */
template<typename T>
concept HasElementType = requires {
    typename T::element_type;
};

/**
 * @brief Helper to get element type from a data container
 *
 * Provides compile-time mapping from container types to their element types.
 * - DigitalEventSeries → EventWithId
 * - AnalogTimeSeries → AnalogTimeSeries::TimeValuePoint
 * - Types with element_type alias → T::element_type
 */
template<typename T>
struct element_type_of {
    // Primary template: attempt to use T::element_type if available
    using type = typename T::element_type;
};

// Specialization: DigitalEventSeries uses EventWithId
template<>
struct element_type_of<DigitalEventSeries> {
    using type = EventWithId;
};

// Specialization: AnalogTimeSeries uses TimeValuePoint
template<>
struct element_type_of<AnalogTimeSeries> {
    using type = AnalogTimeSeries::TimeValuePoint;
};

// Specialization: DigitalIntervalSeries uses IntervalWithId
template<>
struct element_type_of<DigitalIntervalSeries> {
    using type = IntervalWithId;
};

template<typename T>
using element_type_of_t = typename element_type_of<T>::type;

/**
 * @brief Convert an interval from one TimeFrame coordinate system to another.
 *
 * @param interval Interval expressed in @p from_time_frame indices
 * @param from_time_frame TimeFrame that owns @p interval coordinates
 * @param to_time_frame TimeFrame used by the source data being queried
 * @return Interval expressed in @p to_time_frame indices
 *
 * @pre @p from_time_frame and @p to_time_frame are valid TimeFrame references.
 * @post Returned bounds are expressed in @p to_time_frame coordinates.
 */
[[nodiscard]] inline Interval convertIntervalToTimeFrame(
        Interval const & interval,
        TimeFrame const & from_time_frame,
        TimeFrame const & to_time_frame) {
    auto [converted_start, converted_end] = convertTimeFrameRange(
            TimeFrameIndex(interval.start),
            TimeFrameIndex(interval.end),
            from_time_frame,
            to_time_frame);
    return Interval{converted_start.getValue(), converted_end.getValue()};
}

/**
 * @brief Convert prepared gather-window bounds to source-data query bounds.
 *
 * @param interval Window bounds in the interval series TimeFrame
 * @param interval_time_frame Non-null TimeFrame from the interval/window series
 * @param source_time_frame Non-null TimeFrame from the source data
 * @return Query interval in source-data TimeFrame coordinates
 *
 * @pre Prepared gather windows and source data must both carry non-null TimeFrames.
 * @post Returned bounds are expressed in the source data TimeFrame.
 */
[[nodiscard]] inline Interval convertPreparedWindowToSourceInterval(
        Interval const & interval,
        std::shared_ptr<TimeFrame> const & interval_time_frame,
        std::shared_ptr<TimeFrame> const & source_time_frame) {
    assert(interval_time_frame && "convertPreparedWindowToSourceInterval: interval TimeFrame must not be null");
    assert(source_time_frame && "convertPreparedWindowToSourceInterval: source TimeFrame must not be null");

    if (!interval_time_frame || !source_time_frame) {
        throw std::invalid_argument(
                "convertPreparedWindowToSourceInterval: prepared windows and source data must have TimeFrames");
    }

    return convertIntervalToTimeFrame(interval, *interval_time_frame, *source_time_frame);
}

}// namespace Neuralyzer::Gather

// =============================================================================
// GatherResult Template
// =============================================================================

/**
 * @brief Container for collected views created by gather operations
 *
 * GatherResult holds a collection of view-backed (or copied) data objects
 * created by aligning a source to a set of intervals. It provides a range
 * interface for iteration and convenience methods for analysis.
 *
 * @tparam T The data type (e.g., DigitalEventSeries, AnalogTimeSeries)
 */
template<typename T>
class GatherResult {
public:
    using value_type = std::shared_ptr<T>;
    using const_iterator = typename std::vector<value_type>::const_iterator;
    using size_type = typename std::vector<value_type>::size_type;

    /// Element type yielded by view iteration (e.g., EventWithId for DigitalEventSeries)
    using element_type = Neuralyzer::Gather::element_type_of_t<T>;

    // ========== Constructors ==========

    /**
     * @brief Default constructor - creates an empty GatherResult
     */
    GatherResult() = default;

    // ========== Factory Methods ==========

    /**
     * @brief Create GatherResult from source and alignment intervals (TimeFrameIndex version)
     *
     * Creates a view for each interval in the alignment series. Each view
     * references the source data without copying.
     *
     * @param source Source data to create views from
     * @param intervals Alignment intervals (each interval becomes one view)
     * @return GatherResult containing views for each interval
     *
     * @note Requires T to satisfy ViewableDataType concept
     */
    template<typename U = T>
        requires Neuralyzer::Gather::ViewableDataType<U>
    static GatherResult create(
            std::shared_ptr<U> source,
            std::shared_ptr<DigitalIntervalSeries const> windows,
            std::shared_ptr<DigitalEventSeries const> alignment_points = nullptr) {
        return _createFromPreparedWindows<U>(
                std::move(source),
                std::move(windows),
                std::move(alignment_points));
    }

    /**
     * @brief Create GatherResult from source and alignment intervals (int64_t version)
     *
     * Overload for types that use int64_t in their createView signature
     * (e.g., DigitalIntervalSeries).
     *
     * @param source Source data to create views from
     * @param intervals Alignment intervals (each interval becomes one view)
     * @return GatherResult containing views for each interval
     */
    template<typename U = T>
        requires Neuralyzer::Gather::ViewableDataTypeInt64<U> &&
                 (!Neuralyzer::Gather::ViewableDataType<U>)
    static GatherResult create(
            std::shared_ptr<U> source,
            std::shared_ptr<DigitalIntervalSeries const> windows,
            std::shared_ptr<DigitalEventSeries const> alignment_points = nullptr) {
        return _createFromPreparedWindows<U>(
                std::move(source),
                std::move(windows),
                std::move(alignment_points));
    }

    /**
     * @brief Create GatherResult using time range copies (for RaggedTimeSeries types)
     *
     * For types that don't support zero-copy views, this creates owning copies
     * of data within each interval range.
     *
     * @param source Source data to create copies from
     * @param intervals Alignment intervals (each interval becomes one copy)
     * @return GatherResult containing copies for each interval
     */
    template<typename U = T>
        requires Neuralyzer::Gather::CopyableTimeRangeDataType<U> &&
                 (!Neuralyzer::Gather::ViewableDataType<U>) &&
                 (!Neuralyzer::Gather::ViewableDataTypeInt64<U>)
    static GatherResult create(
            std::shared_ptr<U> source,
            std::shared_ptr<DigitalIntervalSeries const> windows,
            std::shared_ptr<DigitalEventSeries const> alignment_points = nullptr) {
        return _createFromPreparedWindows<U>(
                std::move(source),
                std::move(windows),
                std::move(alignment_points));
    }

    /**
     * @brief Create a GatherResult from already-created row DataObjects.
     *
     * @param rows Row-aligned DataObjects in display order
     * @param windows Optional row-window metadata with the same row count
     * @param alignment_points Optional row-aligned alignment events with the same row count
     * @return GatherResult that owns row pointers and optional metadata
     *
     * @pre Every row pointer must be non-null.
     * @pre If supplied, @p windows and @p alignment_points must match @p rows size.
     * @post The returned result has no direct-gather source provenance.
     */
    [[nodiscard]] static GatherResult fromRows(
            std::vector<value_type> rows,
            std::shared_ptr<DigitalIntervalSeries const> windows = nullptr,
            std::shared_ptr<DigitalEventSeries const> alignment_points = nullptr) {
        _validateRowInputs(rows, windows, alignment_points);

        GatherResult result;
        result._views = std::move(rows);
        result._windows = std::move(windows);
        result._alignment_points = std::move(alignment_points);
        result._query_intervals = _queryIntervalsFromWindows(result._windows);
        return result;
    }

    /**
     * @brief Create transformed rows while preserving metadata from another GatherResult.
     *
     * @tparam ParentT Parent row DataObject type
     * @param parent GatherResult that supplies row metadata and ordering
     * @param rows Row DataObjects aligned to the visible order of @p parent
     * @return GatherResult with @p rows and parent row metadata
     *
     * @pre Every row pointer must be non-null.
     * @pre @p rows must have the same row count as @p parent.
     * @post The returned result has no direct-gather source provenance.
     */
    template<typename ParentT>
    [[nodiscard]] static GatherResult fromRowsLike(
            GatherResult<ParentT> const & parent,
            std::vector<value_type> rows) {
        if (rows.size() != parent.size()) {
            throw std::invalid_argument("GatherResult::fromRowsLike: row count must match parent result size");
        }
        _validateRowInputs(rows, parent.windows(), parent.alignmentPoints());

        GatherResult result;
        result._views = std::move(rows);
        result._windows = parent.windows();
        result._alignment_points = parent.alignmentPoints();
        result._query_intervals = parent.intervals();
        if (parent.isReordered()) {
            result._reorder_indices.reserve(parent.size());
            for (size_type i = 0; i < parent.size(); ++i) {
                result._reorder_indices.push_back(parent.originalIndex(i));
            }
        }
        return result;
    }

    // ========== Range Interface ==========

    /**
     * @brief Get iterator to the first view
     */
    [[nodiscard]] const_iterator begin() const noexcept { return _views.begin(); }

    /**
     * @brief Get iterator past the last view
     */
    [[nodiscard]] const_iterator end() const noexcept { return _views.end(); }

    /**
     * @brief Get const iterator to the first view
     */
    [[nodiscard]] const_iterator cbegin() const noexcept { return _views.cbegin(); }

    /**
     * @brief Get const iterator past the last view
     */
    [[nodiscard]] const_iterator cend() const noexcept { return _views.cend(); }

    /**
     * @brief Get the number of views
     */
    [[nodiscard]] size_type size() const noexcept { return _views.size(); }

    /**
     * @brief Check if the result is empty
     */
    [[nodiscard]] bool empty() const noexcept { return _views.empty(); }

    /**
     * @brief Access view at index
     *
     * @param i Index of the view to access
     * @return Reference to the shared_ptr at index i
     */
    [[nodiscard]] value_type const & operator[](size_type i) const { return _views[i]; }

    /**
     * @brief Access view at index with bounds checking
     *
     * @param i Index of the view to access
     * @return Reference to the shared_ptr at index i
     * @throws std::out_of_range if i >= size()
     */
    [[nodiscard]] value_type const & at(size_type i) const { return _views.at(i); }

    /**
     * @brief Get the first view
     */
    [[nodiscard]] value_type const & front() const { return _views.front(); }

    /**
     * @brief Get the last view
     */
    [[nodiscard]] value_type const & back() const { return _views.back(); }

    // ========== Source Access ==========

    /**
     * @brief Get the source data that views were created from
     */
    [[nodiscard]] std::shared_ptr<T> source() { return _source; }

    /**
     * @brief Get the alignment intervals used to create views
     */
    [[nodiscard]] std::vector<Interval> const & intervals() const { return _query_intervals; }

    /**
     * @brief Get the prepared gather-window series used to define rows.
     *
     * @return Shared pointer to the original prepared windows, or nullptr for
     *         empty/default and legacy adapter-only results.
     *
     * @post Return value is shared with the GatherResult and is never modified by it.
     */
    [[nodiscard]] std::shared_ptr<DigitalIntervalSeries const> windows() const noexcept { return _windows; }

    /**
     * @brief Get the companion alignment-point event series.
     *
     * @return Shared pointer to row-aligned alignment events, or nullptr when
     *         the gather has no companion alignment metadata.
     *
     * @post If non-null, the series has the same row count as `windows()`.
     */
    [[nodiscard]] std::shared_ptr<DigitalEventSeries const> alignmentPoints() const noexcept { return _alignment_points; }

    /**
     * @brief Get the interval at a specific index (O(1) access)
     *
     * @param i Index of the interval
     * @return The Interval at index i
     * @throws std::out_of_range if i >= size()
     */
    [[nodiscard]] Interval intervalAt(size_type i) const {
        if (i >= _query_intervals.size()) {
            throw std::out_of_range("GatherResult::intervalAt: index out of range");
        }
        return _query_intervals[i];
    }

    /**
     * @brief Get the alignment time for a specific trial (O(1) access)
     *
     * Returns the time point used for alignment (t=0 reference) for the
     * specified trial. This is the value that should be subtracted from
     * event times to get trial-relative times.
     *
     * Prepared-window gathers with companion alignment points return the
     * companion event time. Gathers without companion alignment metadata return
     * the source-query interval start as a fallback.
     *
     * @param i Index of the trial
     * @return Alignment time for trial i
     * @throws std::out_of_range if i >= size()
     */
    [[nodiscard]] int64_t alignmentTimeAt(size_type i) const {
        if (i >= size()) {
            throw std::out_of_range("GatherResult::alignmentTimeAt: index out of range");
        }
        // Handle potential reordering from sortBy()/reorder()
        size_type orig_idx = !_reorder_indices.empty() ? _reorder_indices[i] : i;

        if (_alignment_points) {
            return _alignmentTimeFromCompanionEvent(orig_idx);
        }

        if (orig_idx >= _query_intervals.size()) {
            throw std::out_of_range("GatherResult::alignmentTimeAt: no alignment metadata for row");
        }
        return _query_intervals[orig_idx].start;
    }

    // ========== Convenience Methods ==========

    /**
     * @brief Get all views as a ranges-compatible view
     */
    [[nodiscard]] auto views() const {
        return std::views::all(_views);
    }

    /**
     * @brief Apply a function to all views and collect results
     *
     * @tparam F Callable type
     * @param func Function to apply to each view (takes value_type const&)
     * @return Vector of results from applying func to each view
     *
     * @example
     * @code
     * auto spike_counts = raster.transform([](auto const& trial) {
     *     return trial->size();
     * });
     * @endcode
     */
    template<typename F>
    [[nodiscard]] auto transform(F && func) const {
        using ResultType = std::invoke_result_t<F, value_type const &>;
        std::vector<ResultType> results;
        results.reserve(_views.size());
        for (auto const & view: _views) {
            results.push_back(std::invoke(std::forward<F>(func), view));
        }
        return results;
    }

    /**
     * @brief Apply a function to all views with interval access
     *
     * @tparam F Callable type
     * @param func Function taking (view, interval) pairs
     * @return Vector of results
     *
     * @example
     * @code
     * auto results = raster.transformWithInterval(
     *     [](auto const& trial, Interval const& interval) {
     *         return std::make_pair(trial->size(), interval.end - interval.start);
     *     });
     * @endcode
     */
    template<typename F>
    [[nodiscard]] auto transformWithInterval(F && func) const {
        using ResultType = std::invoke_result_t<F, value_type const &, Interval const &>;
        std::vector<ResultType> results;
        results.reserve(_views.size());

        for (size_t idx = 0; idx < _views.size(); ++idx) {
            results.push_back(std::invoke(
                    std::forward<F>(func),
                    _views[idx],
                    intervalAt(idx)));
        }
        return results;
    }

    /**
     * @brief Materialize all views into owning storage
     *
     * If views are backed by view storage, this creates a new GatherResult
     * where each element has owning storage (copies the data).
     *
     * @return New GatherResult with materialized views and preserved trial metadata
     */
    [[nodiscard]] GatherResult materialize() const {
        GatherResult result;
        result._source = _source;
        result._windows = _windows;
        result._alignment_points = _alignment_points;
        result._query_intervals = _query_intervals;
        result._reorder_indices = _reorder_indices;
        result._views.reserve(_views.size());

        for (auto const & view: _views) {
            if constexpr (requires { view->materialize(); }) {
                result._views.push_back(view->materialize());
            } else {
                // Type doesn't have materialize(), just copy the shared_ptr
                result._views.push_back(view);
            }
        }

        return result;
    }

    /**
     * @brief Create reordered GatherResult using index permutation
     *
     * Creates a new GatherResult with trials in the order specified by indices.
     * The new result shares the same source data and intervals, but views and
     * iteration order follow the provided permutation.
     *
     * @param indices Permutation of trial indices (must be valid permutation of [0, size()))
     * @return New GatherResult with trials in specified order
     * @throws std::invalid_argument if indices has wrong size
     * @throws std::out_of_range if any index is >= size()
     *
     * @example
     * @code
     * auto sort_order = computeSortOrder(result);
     * auto sorted_result = result.reorder(sort_order);
     *
     * // sorted_result[0] is now the trial with smallest reduction value
     * @endcode
     */
    [[nodiscard]] GatherResult reorder(std::vector<size_type> const & indices) const {
        if (indices.size() != size()) {
            throw std::invalid_argument(
                    "GatherResult::reorder: indices size must match result size");
        }

        GatherResult result;
        result._source = _source;
        // Note: We keep the original intervals - reordering is logical only
        result._windows = _windows;
        result._alignment_points = _alignment_points;
        result._query_intervals = _query_intervals;
        result._views.reserve(size());
        result._reorder_indices = indices;// Store the reorder mapping

        for (auto idx: indices) {
            if (idx >= size()) {
                throw std::out_of_range(
                        "GatherResult::reorder: index out of range");
            }
            result._views.push_back(_views[idx]);
        }

        return result;
    }

    /**
     * @brief Get the original trial index for a position in a reordered result
     *
     * After reordering, this returns the original trial index for a given
     * position in the reordered sequence.
     *
     * @param reordered_idx Index in the reordered result
     * @return Original trial index, or reordered_idx if not reordered
     */
    [[nodiscard]] size_type originalIndex(size_type reordered_idx) const {
        if (reordered_idx >= size()) {
            throw std::out_of_range("GatherResult::originalIndex: index out of range");
        }
        if (_reorder_indices.empty()) {
            return reordered_idx;// Not reordered
        }
        return _reorder_indices[reordered_idx];
    }

    /**
     * @brief Check if this result has been reordered
     */
    [[nodiscard]] bool isReordered() const noexcept {
        return !_reorder_indices.empty();
    }

    /**
     * @brief Get the interval for a position in a reordered result
     *
     * This is the interval from the original trial, not the reordered position.
     * Use originalIndex() to map reordered position to original trial index.
     *
     * @param reordered_idx Index in the (possibly reordered) result
     * @return The Interval for the original trial at this position
     */
    [[nodiscard]] Interval intervalAtReordered(size_type reordered_idx) const {
        return intervalAt(originalIndex(reordered_idx));
    }

private:
    /**
     * @brief Validate row-synthesis inputs and optional row metadata.
     *
     * @pre Every row pointer must be non-null.
     * @pre Optional metadata series must have the same row count as @p rows.
     * @post Throws `std::invalid_argument` if any runtime precondition is violated.
     */
    static void _validateRowInputs(
            std::vector<value_type> const & rows,
            std::shared_ptr<DigitalIntervalSeries const> const & windows,
            std::shared_ptr<DigitalEventSeries const> const & alignment_points) {
        auto const has_null_row = std::ranges::any_of(rows, [](auto const & row) {
            return row == nullptr;
        });

        assert(!has_null_row && "GatherResult::fromRows: rows must not contain null pointers");
        assert((!windows || windows->size() == rows.size()) &&
               "GatherResult::fromRows: window count must match row count");
        assert((!alignment_points || alignment_points->size() == rows.size()) &&
               "GatherResult::fromRows: alignment point count must match row count");

        if (has_null_row) {
            throw std::invalid_argument("GatherResult::fromRows: rows must not contain null pointers");
        }
        if (windows && windows->size() != rows.size()) {
            throw std::invalid_argument("GatherResult::fromRows: window count must match row count");
        }
        if (alignment_points && alignment_points->size() != rows.size()) {
            throw std::invalid_argument("GatherResult::fromRows: alignment point count must match row count");
        }
    }

    /**
     * @brief Extract compatibility interval metadata from prepared windows.
     *
     * @pre @p windows may be null.
     * @post Returns empty metadata when @p windows is null.
     */
    [[nodiscard]] static std::vector<Interval> _queryIntervalsFromWindows(
            std::shared_ptr<DigitalIntervalSeries const> const & windows) {
        std::vector<Interval> intervals;
        if (!windows) {
            return intervals;
        }

        intervals.reserve(windows->size());
        for (auto const & window: windows->view()) {
            intervals.push_back(window.interval);
        }
        return intervals;
    }

    /**
     * @brief Validate source, windows, and optional companion alignment points.
     *
     * @pre `windows` must be non-null. Direct gather creation also requires a non-null source.
     * @post Throws `std::invalid_argument` if any runtime precondition is violated.
     */
    template<typename U>
    static void _validatePreparedGatherInputs(
            std::shared_ptr<U> const & source,
            std::shared_ptr<DigitalIntervalSeries const> const & windows,
            std::shared_ptr<DigitalEventSeries const> const & alignment_points) {
        assert(source && "GatherResult::create: source must not be null");
        assert(windows && "GatherResult::create: windows must not be null");
        assert((!alignment_points || alignment_points->size() == windows->size()) &&
               "GatherResult::create: alignment point count must match window count");

        if (!source || !windows) {
            throw std::invalid_argument("GatherResult::create: source and windows must not be null");
        }
        if (alignment_points && alignment_points->size() != windows->size()) {
            throw std::invalid_argument(
                    "GatherResult::create: alignment point count must match window count");
        }
    }

    /**
     * @brief Create one row DataObject with TimeFrameIndex view bounds.
     *
     * @pre `source` must be non-null and `query_interval` must be expressed in source coordinates.
     * @post Returns a row object created by the DataObject view API.
     */
    template<typename U>
        requires Neuralyzer::Gather::ViewableDataType<U>
    static std::shared_ptr<U> _createRowForQueryInterval(
            std::shared_ptr<U> const & source,
            Interval const & query_interval) {
        return U::createView(
                source,
                TimeFrameIndex(query_interval.start),
                TimeFrameIndex(query_interval.end));
    }

    /**
     * @brief Create one row DataObject with int64 view bounds.
     *
     * @pre `source` must be non-null and `query_interval` must be expressed in source coordinates.
     * @post Returns a row object created by the DataObject view API.
     */
    template<typename U>
        requires Neuralyzer::Gather::ViewableDataTypeInt64<U> &&
                 (!Neuralyzer::Gather::ViewableDataType<U>)
    static std::shared_ptr<U> _createRowForQueryInterval(
            std::shared_ptr<U> const & source,
            Interval const & query_interval) {
        return U::createView(source, query_interval.start, query_interval.end);
    }

    /**
     * @brief Create one row DataObject as an owning time-range copy.
     *
     * @pre `source` must be non-null and `query_interval` must be expressed in source coordinates.
     * @post Returns a row object with source TimeFrame and image-size metadata.
     */
    template<typename U>
        requires Neuralyzer::Gather::CopyableTimeRangeDataType<U> &&
                 (!Neuralyzer::Gather::ViewableDataType<U>) &&
                 (!Neuralyzer::Gather::ViewableDataTypeInt64<U>)
    static std::shared_ptr<U> _createRowForQueryInterval(
            std::shared_ptr<U> const & source,
            Interval const & query_interval) {
        auto copy = std::make_shared<U>(source->createTimeRangeCopy(
                TimeFrameIndex(query_interval.start),
                TimeFrameIndex(query_interval.end)));
        copy->setTimeFrame(source->getTimeFrame());
        copy->setImageSize(source->getImageSize());
        return copy;
    }

    /**
     * @brief Create a GatherResult from prepared windows and optional alignment points.
     *
     * @pre `source` and `windows` must be non-null. If supplied, `alignment_points`
     *      must have the same row count as `windows`.
     * @post Rows are created from source-query intervals while prepared metadata is retained.
     */
    template<typename U>
    static GatherResult _createFromPreparedWindows(
            std::shared_ptr<U> source,
            std::shared_ptr<DigitalIntervalSeries const> windows,
            std::shared_ptr<DigitalEventSeries const> alignment_points) {
        _validatePreparedGatherInputs(source, windows, alignment_points);

        GatherResult result;
        result._source = std::move(source);
        result._windows = std::move(windows);
        result._alignment_points = std::move(alignment_points);
        result._views.reserve(result._windows->size());
        result._query_intervals.reserve(result._windows->size());

        auto const source_tf = result._source->getTimeFrame();
        auto const window_tf = result._windows->getTimeFrame();

        for (auto const & window: result._windows->view()) {
            auto const query_interval = Neuralyzer::Gather::convertPreparedWindowToSourceInterval(
                    window.interval,
                    window_tf,
                    source_tf);
            result._query_intervals.push_back(query_interval);
            result._views.push_back(_createRowForQueryInterval<U>(result._source, query_interval));
        }

        return result;
    }

    /**
     * @brief Resolve a companion alignment event for an original row index.
     *
     * @pre `_alignment_points` must be non-null and `original_idx` must be in range.
     * @post Returned time is expressed in physical time units when the alignment series has a TimeFrame.
     */
    [[nodiscard]] int64_t _alignmentTimeFromCompanionEvent(size_type original_idx) const {
        assert(_alignment_points && "GatherResult::alignmentTimeAt: alignment points must exist");
        assert(original_idx < _alignment_points->size() &&
               "GatherResult::alignmentTimeAt: row index must be in range");

        auto const alignment_view = _alignment_points->view();
        auto const alignment_time = alignment_view[original_idx].time();
        auto const alignment_tf = _alignment_points->getTimeFrame();

        return alignment_tf ? alignment_tf->getTimeAtIndex(alignment_time) : alignment_time.getValue();
    }

    std::shared_ptr<T> _source;
    std::shared_ptr<DigitalIntervalSeries const> _windows;
    std::shared_ptr<DigitalEventSeries const> _alignment_points;
    std::vector<Interval> _query_intervals;// Source-coordinate intervals used for row creation
    std::vector<value_type> _views;
    std::vector<size_type> _reorder_indices;// Maps reordered position → original index
};

// =============================================================================
// Free Function: gather()
// =============================================================================

/**
 * @brief Create a GatherResult from source data and alignment intervals
 *
 * This is the primary interface for gather operations. It creates a collection
 * of views (or copies) of the source data, one for each alignment interval.
 *
 * @tparam T The source data type
 * @param source Source data to create views from
 * @param intervals Alignment intervals defining the view boundaries
 * @return GatherResult containing one view/copy per interval
 *
 * @example
 * @code
 * // Raster plot: align spikes to trial starts
 * auto spikes = dm->getData<DigitalEventSeries>("spikes");
 * auto trials = dm->getData<DigitalIntervalSeries>("trials");
 * auto raster = gather(spikes, trials);
 *
 * // Process each trial
 * for (size_t i = 0; i < raster.size(); ++i) {
 *     auto& trial = raster[i];
 *     Interval interval = raster.intervalAt(i);
 *     // trial is std::shared_ptr<DigitalEventSeries>
 * }
 * @endcode
 */
template<typename T>
[[nodiscard]] GatherResult<T> gather(
        std::shared_ptr<T> source,
        std::shared_ptr<DigitalIntervalSeries const> windows) {
    return GatherResult<T>::create(std::move(source), std::move(windows));
}

/**
 * @brief Create a GatherResult from source data, prepared windows, and alignment points.
 *
 * @tparam T The source data type
 * @param source Source data to create rows from
 * @param windows Prepared gather windows defining the row bounds
 * @param alignment_points Optional row-aligned alignment events used for trial-relative metadata
 * @return GatherResult containing one row per window
 *
 * @pre `source` and `windows` must be non-null.
 * @pre If non-null, `alignment_points` must have the same row count as `windows`.
 * @post Prepared windows and alignment points are retained as row metadata.
 */
template<typename T>
[[nodiscard]] GatherResult<T> gather(
        std::shared_ptr<T> source,
        std::shared_ptr<DigitalIntervalSeries const> windows,
        std::shared_ptr<DigitalEventSeries const> alignment_points) {
    return GatherResult<T>::create(
            std::move(source),
            std::move(windows),
            std::move(alignment_points));
}

#endif// GATHER_RESULT_HPP
