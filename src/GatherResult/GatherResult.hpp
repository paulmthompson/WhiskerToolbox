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

#include "IntervalAdapters.hpp"

#include "ViewAdaptorTypes.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"
#include "TransformsV2/PipelineValueStore/PipelineValueStore.hpp"
#include "TransformsV2/extension/ValueProjectionTypes.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <concepts>
#include <functional>
#include <iterator>
#include <memory>
#include <numeric>
#include <ranges>
#include <stdexcept>
#include <type_traits>
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
 * @brief Concept for types that provide TimeFrame access
 *
 * Interval source adapters (like EventExpanderAdapter, IntervalWithAlignmentAdapter)
 * can provide their underlying TimeFrame for cross-timeframe alignment.
 */
template<typename T>
concept HasTimeFrameAccess = requires(T const & t) {
    { t.getTimeFrame() } -> std::same_as<std::shared_ptr<TimeFrame>>;
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

/**
 * @brief Resolve legacy adapter interval bounds to source-data query bounds.
 *
 * @param aligned_interval Adapter-provided bounds and alignment time
 * @param adapter_time_frame TimeFrame for adapter index coordinates, or null for legacy absolute-time bounds
 * @param source_time_frame TimeFrame for source data, or null for legacy raw-index source data
 * @return Query interval in source-data coordinates when possible
 *
 * @pre @p aligned_interval contains valid start/end bounds for its adapter contract.
 * @post Returned bounds preserve the legacy adapter null-TimeFrame behavior.
 */
[[nodiscard]] inline Interval resolveLegacyAdapterIntervalForSource(
        AlignedInterval const & aligned_interval,
        TimeFrame const * adapter_time_frame,
        TimeFrame const * source_time_frame) {
    if (adapter_time_frame && source_time_frame) {
        return convertIntervalToTimeFrame(
                Interval{aligned_interval.start, aligned_interval.end},
                *adapter_time_frame,
                *source_time_frame);
    }

    if (adapter_time_frame) {
        return Interval{
                adapter_time_frame->getTimeAtIndex(TimeFrameIndex(aligned_interval.start)),
                adapter_time_frame->getTimeAtIndex(TimeFrameIndex(aligned_interval.end))};
    }

    if (source_time_frame) {
        return Interval{
                source_time_frame->getIndexAtTime(static_cast<float>(aligned_interval.start), false).getValue(),
                source_time_frame->getIndexAtTime(static_cast<float>(aligned_interval.end)).getValue()};
    }

    return Interval{aligned_interval.start, aligned_interval.end};
}

/**
 * @brief Resolve legacy adapter alignment time to the absolute-time value stored by GatherResult.
 *
 * @param alignment_time Adapter-provided alignment value
 * @param adapter_time_frame TimeFrame for adapter index coordinates, or null for legacy absolute-time values
 * @return Absolute alignment time under the legacy adapter contract
 *
 * @pre @p alignment_time is valid in the adapter coordinate system.
 * @post Return value matches the existing adapter-backed alignmentTimeAt() behavior.
 */
[[nodiscard]] inline int64_t resolveLegacyAdapterAlignmentTime(
        int64_t alignment_time,
        TimeFrame const * adapter_time_frame) {
    if (!adapter_time_frame) {
        return alignment_time;
    }
    return adapter_time_frame->getTimeAtIndex(TimeFrameIndex(alignment_time));
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

    // ========== Factory Methods for Interval Adapters ==========

    /**
     * @brief Create GatherResult from source and an IntervalSource adapter
     *
     * This overload accepts any type satisfying the IntervalSource concept,
     * including EventExpanderAdapter and IntervalWithAlignmentAdapter.
     *
     * The adapter provides AlignedInterval elements which contain:
     * - start/end: interval bounds for view creation
     * - alignment_time: custom alignment point for projections
     *
     * Adapter bounds are resolved by the legacy adapter compatibility path.
     * Prefer prepared DigitalIntervalSeries windows for new code.
     *
     * @tparam IntervalSourceT Type satisfying IntervalSource concept
     * @param source Source data to create views from
     * @param interval_source Adapter providing intervals with alignment info
     * @return GatherResult containing views for each interval
     *
     * @example
     * @code
     * // Cross-timeframe alignment: events at 500Hz, spikes at 30kHz
     * auto events = dm->getData<DigitalEventSeries>("behavior_events");  // 500Hz
     * auto spikes = dm->getData<DigitalEventSeries>("neural_spikes");    // 30kHz
     * auto adapter = expandEvents(events, 15, 15);  // ±15 indices in 500Hz
     * // GatherResult automatically converts to 30kHz indices
     * auto result = GatherResult<DigitalEventSeries>::create(spikes, adapter);
     * @endcode
     */
    template<typename U = T, typename IntervalSourceT>
        requires Neuralyzer::Gather::ViewableDataType<U> &&
                 Neuralyzer::Gather::IntervalSource<IntervalSourceT>
    static GatherResult create(
            std::shared_ptr<U> source,
            IntervalSourceT const & interval_source) {
        GatherResult result;
        result._source = source;
        result._views.reserve(interval_source.size());
        result._query_intervals.reserve(interval_source.size());
        result._legacy_alignment_times.reserve(interval_source.size());

        // Resolve TimeFrames for absolute-time conversion
        auto source_tf = source ? source->getTimeFrame() : nullptr;
        std::shared_ptr<TimeFrame> adapter_tf = nullptr;
        if constexpr (Neuralyzer::Gather::HasTimeFrameAccess<IntervalSourceT>) {
            adapter_tf = interval_source.getTimeFrame();
        }

        for (auto const & aligned_interval: interval_source) {
            auto const query_interval = Neuralyzer::Gather::resolveLegacyAdapterIntervalForSource(
                    aligned_interval,
                    adapter_tf.get(),
                    source_tf.get());
            auto const alignment_abs_time = Neuralyzer::Gather::resolveLegacyAdapterAlignmentTime(
                    aligned_interval.alignment_time,
                    adapter_tf.get());

            result._query_intervals.push_back(query_interval);
            result._legacy_alignment_times.push_back(alignment_abs_time);

            auto view = U::createView(
                    source,
                    TimeFrameIndex(query_interval.start),
                    TimeFrameIndex(query_interval.end));
            result._views.push_back(std::move(view));
        }

        return result;
    }

    /**
     * @brief Create GatherResult from source and IntervalSource (int64_t version)
     */
    template<typename U = T, typename IntervalSourceT>
        requires Neuralyzer::Gather::ViewableDataTypeInt64<U> &&
                 (!Neuralyzer::Gather::ViewableDataType<U>) &&
                 Neuralyzer::Gather::IntervalSource<IntervalSourceT>
    static GatherResult create(
            std::shared_ptr<U> source,
            IntervalSourceT const & interval_source) {
        GatherResult result;
        result._source = source;
        result._views.reserve(interval_source.size());
        result._query_intervals.reserve(interval_source.size());
        result._legacy_alignment_times.reserve(interval_source.size());

        // Resolve TimeFrames for absolute-time conversion
        auto source_tf = source ? source->getTimeFrame() : nullptr;
        std::shared_ptr<TimeFrame> adapter_tf = nullptr;
        if constexpr (Neuralyzer::Gather::HasTimeFrameAccess<IntervalSourceT>) {
            adapter_tf = interval_source.getTimeFrame();
        }

        for (auto const & aligned_interval: interval_source) {
            auto const query_interval = Neuralyzer::Gather::resolveLegacyAdapterIntervalForSource(
                    aligned_interval,
                    adapter_tf.get(),
                    source_tf.get());
            auto const alignment_abs_time = Neuralyzer::Gather::resolveLegacyAdapterAlignmentTime(
                    aligned_interval.alignment_time,
                    adapter_tf.get());

            result._query_intervals.push_back(query_interval);
            result._legacy_alignment_times.push_back(alignment_abs_time);

            auto view = U::createView(
                    source,
                    query_interval.start,
                    query_interval.end);
            result._views.push_back(std::move(view));
        }

        return result;
    }

    /**
     * @brief Create GatherResult from source and IntervalSource (copy version)
     */
    template<typename U = T, typename IntervalSourceT>
        requires Neuralyzer::Gather::CopyableTimeRangeDataType<U> &&
                 (!Neuralyzer::Gather::ViewableDataType<U>) &&
                 (!Neuralyzer::Gather::ViewableDataTypeInt64<U>) &&
                 Neuralyzer::Gather::IntervalSource<IntervalSourceT>
    static GatherResult create(
            std::shared_ptr<U> source,
            IntervalSourceT const & interval_source) {
        GatherResult result;
        result._source = source;
        result._views.reserve(interval_source.size());
        result._query_intervals.reserve(interval_source.size());
        result._legacy_alignment_times.reserve(interval_source.size());

        // Resolve TimeFrames for absolute-time conversion
        auto source_tf = source ? source->getTimeFrame() : nullptr;
        std::shared_ptr<TimeFrame> adapter_tf = nullptr;
        if constexpr (Neuralyzer::Gather::HasTimeFrameAccess<IntervalSourceT>) {
            adapter_tf = interval_source.getTimeFrame();
        }

        for (auto const & aligned_interval: interval_source) {
            auto const query_interval = Neuralyzer::Gather::resolveLegacyAdapterIntervalForSource(
                    aligned_interval,
                    adapter_tf.get(),
                    source_tf.get());
            auto const alignment_abs_time = Neuralyzer::Gather::resolveLegacyAdapterAlignmentTime(
                    aligned_interval.alignment_time,
                    adapter_tf.get());

            result._query_intervals.push_back(query_interval);
            result._legacy_alignment_times.push_back(alignment_abs_time);

            auto copy = std::make_shared<U>(source->createTimeRangeCopy(
                    TimeFrameIndex(query_interval.start),
                    TimeFrameIndex(query_interval.end)));
            copy->setTimeFrame(source->getTimeFrame());
            copy->setImageSize(source->getImageSize());
            result._views.push_back(std::move(copy));
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
     * For GatherResults created with:
     * - IntervalWithAlignmentAdapter: Returns start, end, or center based on alignment setting
     * - EventExpanderAdapter: Returns the event time (center of the expanded window)
     * - Basic gather(): Returns interval.start as fallback
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

        // Use legacy adapter alignment times if available, otherwise fall back to interval start.
        if (!_legacy_alignment_times.empty() && orig_idx < _legacy_alignment_times.size()) {
            return _legacy_alignment_times[orig_idx];
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
                    _query_intervals[idx]));
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
        result._legacy_alignment_times = _legacy_alignment_times;
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

    // ========== Pipeline Integration Methods ==========

    /**
     * @brief Build value store for a specific trial (V2 pattern)
     *
     * Creates a PipelineValueStore populated with standard trial values that can
     * be bound to transform parameters. This enables generic parameter binding
     * without specialized context structs.
     *
     * ## Store Keys
     *
     * - "alignment_time": int64_t - Trial start time (used as t=0 reference)
     * - "trial_index": int64_t - Original trial index (0-based)
     * - "trial_duration": int64_t - Duration (end - start)
     * - "end_time": int64_t - Trial end time
     *
     * ## Usage with Pipeline Bindings
     *
     * @code
     * // Pipeline with bindings
     * // {"transform": "NormalizeTimeV2", "bindings": {"alignment_time": "alignment_time"}}
     * auto factory = bindValueProjectionV2<EventWithId, float>(pipeline);
     *
     * for (size_t i = 0; i < result.size(); ++i) {
     *     auto store = result.buildTrialStore(i);
     *     auto projection = factory(store);
     *     // Use projection on trial events...
     * }
     * @endcode
     *
     * @param trial_idx Index of the trial (0-based, respects reordering)
     * @return PipelineValueStore populated with trial values
     * @throws std::out_of_range if trial_idx >= size()
     *
     * @see PipelineValueStore for store documentation
     * @see project() for applying store-based projections to all trials
     */
    [[nodiscard]] Neuralyzer::Transforms::V2::PipelineValueStore buildTrialStore(size_type trial_idx) const {
        if (trial_idx >= size()) {
            throw std::out_of_range("GatherResult::buildTrialStore: index out of range");
        }

        auto interval = intervalAtReordered(trial_idx);
        int64_t alignment_time = alignmentTimeAt(trial_idx);
        size_type orig_idx = originalIndex(trial_idx);

        Neuralyzer::Transforms::V2::PipelineValueStore store;
        store.set("alignment_time", alignment_time);
        store.set("trial_index", static_cast<int64_t>(orig_idx));
        store.set("trial_duration", interval.end - interval.start);
        store.set("end_time", static_cast<int64_t>(interval.end));

        return store;
    }

    /**
     * @brief Project values across all trials using value store bindings
     *
     * Creates per-trial value projections using a pipeline that normalizes or
     * transforms element properties (e.g., time normalization). The projection
     * factory receives a value store populated with trial values and applies
     * parameter bindings to produce per-trial projections.
     *
     * @tparam Value The projected value type (e.g., float for normalized time)
     * @param factory Store-based projection factory from bindValueProjectionV2()
     * @return Vector of projection functions, one per trial
     *
     * @example
     * @code
     * // Pipeline with param bindings
     * auto factory = bindValueProjectionV2<EventWithId, float>(pipeline);
     * auto projections = result.project(factory);
     *
     * for (size_t i = 0; i < result.size(); ++i) {
     *     auto const& projection = projections[i];
     *     for (auto const& event : result[i]->view()) {
     *         float norm_time = projection(event);
     *         EntityId id = event.id();
     *         draw_point(norm_time, i, id);
     *     }
     * }
     * @endcode
     *
     * @see buildTrialStore() for store population
     * @see bindValueProjectionV2() for creating factories
     */
    template<typename Value>
    [[nodiscard]] auto project(
            Neuralyzer::Transforms::V2::ValueProjectionFactoryV2<element_type, Value> const & factory) const {
        using ProjectionFn = Neuralyzer::Transforms::V2::ValueProjectionFn<element_type, Value>;
        std::vector<ProjectionFn> projections;
        projections.reserve(size());

        for (size_type i = 0; i < size(); ++i) {
            auto store = buildTrialStore(i);
            projections.push_back(factory(store));
        }

        return projections;
    }

    /**
     * @brief Apply reduction across all trials using value store bindings
     *
     * Executes a range reduction on each trial's view, producing a scalar per trial.
     * The reducer factory is called once per trial with the trial's value store,
     * enabling context-aware reductions (e.g., counting events after alignment).
     *
     * @tparam Scalar Result type of reduction (e.g., int for count, float for latency)
     * @param reducer_factory Store-based reducer factory
     * @return Vector of reduction results, one per trial
     *
     * @example
     * @code
     * auto factory = [](PipelineValueStore const& store) -> ReducerFn<EventWithId, float> {
     *     int64_t alignment = store.getInt("alignment_time").value();
     *     return [alignment](std::span<EventWithId const> events) -> float {
     *         if (events.empty()) return NaN;
     *         return static_cast<float>(events[0].time().getValue() - alignment);
     *     };
     * };
     * auto latencies = result.reduce(factory);
     * @endcode
     *
     * @note This requires T to have a view() method that returns a range
     * @see buildTrialStore() for store population
     */
    template<typename Scalar>
    [[nodiscard]] std::vector<Scalar> reduce(
            Neuralyzer::Gather::ReducerFactoryV2<element_type, Scalar> const & reducer_factory) const {
        std::vector<Scalar> results;
        results.reserve(size());

        for (size_type i = 0; i < size(); ++i) {
            auto store = buildTrialStore(i);
            auto reducer = reducer_factory(store);

            // Materialize view into vector for reducer (takes span)
            auto view = _views[i]->view();
            std::vector<element_type> elements(view.begin(), view.end());

            results.push_back(reducer(std::span<element_type const>{elements}));
        }

        return results;
    }

    /**
     * @brief Get sort indices by reduction result
     *
     * Computes a reduction for each trial and returns the indices that would
     * sort the trials by their reduction values. Useful for sorting trials
     * by first-spike latency, event count, or other metrics.
     *
     * @tparam Scalar Reduction result type (must be comparable)
     * @param reducer_factory Store-based reducer factory
     * @param ascending Sort order (true = smallest first, false = largest first)
     * @return Vector of indices that would sort trials by reduction result
     *
     * @example
     * @code
     * // Sort trials by first-spike latency (ascending)
     * auto factory = [](PipelineValueStore const& store) { ... };
     * auto sort_order = result.sortIndicesBy(factory, true);
     *
     * // Draw trials in sorted order
     * for (size_t row = 0; row < sort_order.size(); ++row) {
     *     size_t trial_idx = sort_order[row];
     *     // Draw trial trial_idx at row position...
     * }
     * @endcode
     *
     * @see reduce() for the underlying reduction
     * @see reorder() for creating a reordered GatherResult
     */
    template<typename Scalar>
    [[nodiscard]] std::vector<size_type> sortIndicesBy(
            Neuralyzer::Gather::ReducerFactoryV2<element_type, Scalar> const & reducer_factory,
            bool ascending = true) const {

        auto values = reduce(reducer_factory);

        std::vector<size_type> indices(size());
        std::iota(indices.begin(), indices.end(), size_type{0});

        if (ascending) {
            std::stable_sort(indices.begin(), indices.end(),
                             [&values](size_type a, size_type b) {
                                 // Handle NaN: NaN values sort to end
                                 if constexpr (std::is_floating_point_v<Scalar>) {
                                     if (std::isnan(values[a])) return false;
                                     if (std::isnan(values[b])) return true;
                                 }
                                 return values[a] < values[b];
                             });
        } else {
            std::stable_sort(indices.begin(), indices.end(),
                             [&values](size_type a, size_type b) {
                                 // Handle NaN: NaN values sort to end
                                 if constexpr (std::is_floating_point_v<Scalar>) {
                                     if (std::isnan(values[a])) return false;
                                     if (std::isnan(values[b])) return true;
                                 }
                                 return values[a] > values[b];
                             });
        }

        return indices;
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
     * auto sort_order = result.sortIndicesBy(reducer_factory, true);
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
        result._legacy_alignment_times = _legacy_alignment_times;
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
    std::vector<size_type> _reorder_indices;     // Maps reordered position → original index
    std::vector<int64_t> _legacy_alignment_times;// Adapter compatibility until IntervalAdapters is removed
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

// =============================================================================
// Free Functions: gather() with Interval Adapters
// =============================================================================

/**
 * @brief Create a GatherResult using an IntervalSource adapter
 *
 * Accepts any type satisfying the IntervalSource concept, including:
 * - EventExpanderAdapter: expands DigitalEventSeries to intervals
 * - IntervalWithAlignmentAdapter: uses custom alignment from DigitalIntervalSeries
 *
 * @tparam T The source data type
 * @tparam IntervalSourceT Type satisfying IntervalSource concept
 * @param source Source data to create views from
 * @param interval_source Adapter providing intervals with alignment info
 * @return GatherResult containing one view/copy per interval
 *
 * @example
 * @code
 * // Expand events to intervals (each event ± 50 frames)
 * auto stimulus_events = dm->getData<DigitalEventSeries>("stimuli");
 * auto spikes = dm->getData<DigitalEventSeries>("spikes");
 * auto raster = gather(spikes, expandEvents(stimulus_events, 50, 50));
 *
 * // Use interval ends as alignment points
 * auto trials = dm->getData<DigitalIntervalSeries>("trials");
 * auto raster = gather(spikes, withAlignment(trials, AlignmentPoint::End));
 * @endcode
 */
template<typename T, typename IntervalSourceT>
    requires Neuralyzer::Gather::IntervalSource<IntervalSourceT>
[[nodiscard]] GatherResult<T> gather(
        std::shared_ptr<T> source,
        IntervalSourceT & interval_source) {
    return GatherResult<T>::create(source, interval_source);
}

#endif// GATHER_RESULT_HPP
