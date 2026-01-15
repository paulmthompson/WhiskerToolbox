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
#include "TimeFrame/interval_data.hpp"
#include "transforms/v2/core/ContextAwareParams.hpp"
#include "transforms/v2/core/ValueProjectionTypes.hpp"
#include "transforms/v2/core/ViewAdaptorTypes.hpp"

#include <algorithm>
#include <cmath>
#include <concepts>
#include <iterator>
#include <memory>
#include <numeric>
#include <ranges>
#include <type_traits>
#include <vector>

// =============================================================================
// Concepts for View Creation
// =============================================================================

namespace WhiskerToolbox::Gather {

/**
 * @brief Concept for types that support zero-copy view creation
 *
 * Requires a static createView() method that takes a source shared_ptr
 * and start/end TimeFrameIndex values.
 */
template<typename T>
concept ViewableDataType = requires(
        std::shared_ptr<T const> source,
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
        std::shared_ptr<T const> source,
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
        T const& source,
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

}// namespace WhiskerToolbox::Gather

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
    using element_type = WhiskerToolbox::Gather::element_type_of_t<T>;

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
        requires WhiskerToolbox::Gather::ViewableDataType<U>
    static GatherResult create(
            std::shared_ptr<U const> source,
            std::shared_ptr<DigitalIntervalSeries const> intervals) {
        GatherResult result;
        result._source = source;
        result._intervals = intervals;
        result._views.reserve(intervals->size());

        for (auto const & interval : intervals->view()) {
            auto view = U::createView(
                    source,
                    TimeFrameIndex(interval.interval.start),
                    TimeFrameIndex(interval.interval.end));
            result._views.push_back(std::move(view));
        }

        return result;
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
        requires WhiskerToolbox::Gather::ViewableDataTypeInt64<U> &&
                 (!WhiskerToolbox::Gather::ViewableDataType<U>)
    static GatherResult create(
            std::shared_ptr<U const> source,
            std::shared_ptr<DigitalIntervalSeries const> intervals) {
        GatherResult result;
        result._source = source;
        result._intervals = intervals;
        result._views.reserve(intervals->size());

        for (auto const & interval : intervals->view()) {
            auto view = U::createView(
                    source,
                    interval.interval.start,
                    interval.interval.end);
            result._views.push_back(std::move(view));
        }

        return result;
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
        requires WhiskerToolbox::Gather::CopyableTimeRangeDataType<U> &&
                 (!WhiskerToolbox::Gather::ViewableDataType<U>) &&
                 (!WhiskerToolbox::Gather::ViewableDataTypeInt64<U>)
    static GatherResult create(
            std::shared_ptr<U const> source,
            std::shared_ptr<DigitalIntervalSeries const> intervals) {
        GatherResult result;
        result._source = source;
        result._intervals = intervals;
        result._views.reserve(intervals->size());

        for (auto const & interval : intervals->view()) {
            auto copy = std::make_shared<U>(source->createTimeRangeCopy(
                    TimeFrameIndex(interval.interval.start),
                    TimeFrameIndex(interval.interval.end)));
            // Inherit TimeFrame and ImageSize from source
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
    [[nodiscard]] std::shared_ptr<T const> source() const { return _source; }

    /**
     * @brief Get the alignment intervals used to create views
     */
    [[nodiscard]] std::shared_ptr<DigitalIntervalSeries const> intervals() const { return _intervals; }

    /**
     * @brief Get the interval at a specific index
     *
     * @param i Index of the interval
     * @return The Interval at index i
     */
    [[nodiscard]] Interval intervalAt(size_type i) const {
        size_t idx = 0;
        for (auto const & interval : _intervals->view()) {
            if (idx == i) {
                return interval.interval;
            }
            ++idx;
        }
        throw std::out_of_range("GatherResult::intervalAt: index out of range");
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
        for (auto const & view : _views) {
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

        size_t idx = 0;
        for (auto const & interval : _intervals->view()) {
            results.push_back(std::invoke(
                    std::forward<F>(func),
                    _views[idx],
                    interval.interval));
            ++idx;
        }
        return results;
    }

    /**
     * @brief Materialize all views into owning storage
     *
     * If views are backed by view storage, this creates a new GatherResult
     * where each element has owning storage (copies the data).
     *
     * @return New GatherResult with materialized views
     */
    [[nodiscard]] GatherResult materialize() const {
        GatherResult result;
        result._source = _source;
        result._intervals = _intervals;
        result._views.reserve(_views.size());

        for (auto const & view : _views) {
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
     * @brief Build TrialContext for a specific trial index
     *
     * Creates a context object that can be used with context-aware pipeline
     * transforms. The context includes alignment time (trial start), trial
     * index, duration, and end time.
     *
     * @param trial_idx Index of the trial
     * @return TrialContext with alignment_time, trial_index, etc.
     * @throws std::out_of_range if trial_idx >= size()
     *
     * @example
     * @code
     * auto projection_factory = bindValueProjectionWithContext<EventWithId, float>(pipeline);
     *
     * for (size_t i = 0; i < result.size(); ++i) {
     *     auto ctx = result.buildContext(i);
     *     auto projection = projection_factory(ctx);
     *     // Use projection on trial events...
     * }
     * @endcode
     */
    [[nodiscard]] WhiskerToolbox::Transforms::V2::TrialContext buildContext(size_type trial_idx) const {
        if (trial_idx >= size()) {
            throw std::out_of_range("GatherResult::buildContext: index out of range");
        }
        // Get the interval for the original trial at this position
        // If reordered, we need the interval from the original trial, not the position
        auto interval = intervalAtReordered(trial_idx);
        size_type orig_idx = originalIndex(trial_idx);
        return WhiskerToolbox::Transforms::V2::TrialContext{
            .alignment_time = TimeFrameIndex(interval.start),
            .trial_index = orig_idx,
            .trial_duration = interval.end - interval.start,
            .end_time = TimeFrameIndex(interval.end)
        };
    }

    /**
     * @brief Project values across all trials using a context-aware pipeline
     *
     * This method enables lazy, per-trial value projection using a pipeline
     * that normalizes or transforms element properties (e.g., time normalization).
     * The projection factory is called once per trial with the trial's context,
     * producing a projection function for that trial.
     *
     * @tparam Value The projected value type (e.g., float for normalized time)
     * @param factory Context-aware projection factory from bindValueProjectionWithContext()
     * @return Vector of projection functions, one per trial
     *
     * @example
     * @code
     * auto factory = bindValueProjectionWithContext<EventWithId, float>(pipeline);
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
     */
    template<typename Value>
    [[nodiscard]] auto project(
            WhiskerToolbox::Transforms::V2::ValueProjectionFactory<element_type, Value> const& factory) const {
        using ProjectionFn = WhiskerToolbox::Transforms::V2::ValueProjectionFn<element_type, Value>;
        std::vector<ProjectionFn> projections;
        projections.reserve(size());

        for (size_type i = 0; i < size(); ++i) {
            auto ctx = buildContext(i);
            projections.push_back(factory(ctx));
        }

        return projections;
    }

    /**
     * @brief Apply reduction across all trials
     *
     * Executes a range reduction on each trial's view, producing a scalar per trial.
     * The reducer factory is called once per trial with the trial's context, enabling
     * context-aware reductions (e.g., counting events after alignment).
     *
     * @tparam Scalar Result type of reduction (e.g., int for count, float for latency)
     * @param reducer_factory Context-aware reducer factory from bindReducerWithContext()
     * @return Vector of reduction results, one per trial
     *
     * @example
     * @code
     * auto factory = bindReducerWithContext<EventWithId, float>(pipeline);
     * auto latencies = result.reduce(factory);
     *
     * // latencies[i] is the first-spike latency for trial i
     * @endcode
     *
     * @note This requires T to have a view() method that returns a range
     */
    template<typename Scalar>
    [[nodiscard]] std::vector<Scalar> reduce(
            WhiskerToolbox::Transforms::V2::ReducerFactory<element_type, Scalar> const& reducer_factory) const {
        std::vector<Scalar> results;
        results.reserve(size());

        for (size_type i = 0; i < size(); ++i) {
            auto ctx = buildContext(i);
            auto reducer = reducer_factory(ctx);

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
     * @param reducer_factory Context-aware reducer factory
     * @param ascending Sort order (true = smallest first, false = largest first)
     * @return Vector of indices that would sort trials by reduction result
     *
     * @example
     * @code
     * // Sort trials by first-spike latency (ascending)
     * auto factory = bindReducerWithContext<EventWithId, float>(pipeline);
     * auto sort_order = result.sortIndicesBy(factory, true);
     *
     * // Draw trials in sorted order
     * for (size_t row = 0; row < sort_order.size(); ++row) {
     *     size_t trial_idx = sort_order[row];
     *     // Draw trial trial_idx at row position...
     * }
     * @endcode
     */
    template<typename Scalar>
    [[nodiscard]] std::vector<size_type> sortIndicesBy(
            WhiskerToolbox::Transforms::V2::ReducerFactory<element_type, Scalar> const& reducer_factory,
            bool ascending = true) const {
        
        auto values = reduce(reducer_factory);

        std::vector<size_type> indices(size());
        std::iota(indices.begin(), indices.end(), size_type{0});

        if (ascending) {
            std::sort(indices.begin(), indices.end(),
                [&values](size_type a, size_type b) {
                    // Handle NaN: NaN values sort to end
                    if constexpr (std::is_floating_point_v<Scalar>) {
                        if (std::isnan(values[a])) return false;
                        if (std::isnan(values[b])) return true;
                    }
                    return values[a] < values[b];
                });
        } else {
            std::sort(indices.begin(), indices.end(),
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
    [[nodiscard]] GatherResult reorder(std::vector<size_type> const& indices) const {
        if (indices.size() != size()) {
            throw std::invalid_argument(
                "GatherResult::reorder: indices size must match result size");
        }

        GatherResult result;
        result._source = _source;
        // Note: We keep the original intervals - reordering is logical only
        result._intervals = _intervals;
        result._views.reserve(size());
        result._reorder_indices = indices;  // Store the reorder mapping

        for (auto idx : indices) {
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
            return reordered_idx;  // Not reordered
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
    std::shared_ptr<T const> _source;
    std::shared_ptr<DigitalIntervalSeries const> _intervals;
    std::vector<value_type> _views;
    std::vector<size_type> _reorder_indices;  // Maps reordered position → original index
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
        std::shared_ptr<T const> source,
        std::shared_ptr<DigitalIntervalSeries const> intervals) {
    return GatherResult<T>::create(source, intervals);
}

/**
 * @brief Create a GatherResult from mutable source (convenience overload)
 *
 * Accepts non-const shared_ptr for convenience.
 */
template<typename T>
[[nodiscard]] GatherResult<T> gather(
        std::shared_ptr<T> source,
        std::shared_ptr<DigitalIntervalSeries const> intervals) {
    return GatherResult<T>::create(
            std::const_pointer_cast<T const>(source),
            intervals);
}

/**
 * @brief Create a GatherResult with mutable intervals (convenience overload)
 */
template<typename T>
[[nodiscard]] GatherResult<T> gather(
        std::shared_ptr<T const> source,
        std::shared_ptr<DigitalIntervalSeries> intervals) {
    return GatherResult<T>::create(
            source,
            std::const_pointer_cast<DigitalIntervalSeries const>(intervals));
}

/**
 * @brief Create a GatherResult with both mutable (convenience overload)
 */
template<typename T>
[[nodiscard]] GatherResult<T> gather(
        std::shared_ptr<T> source,
        std::shared_ptr<DigitalIntervalSeries> intervals) {
    return GatherResult<T>::create(
            std::const_pointer_cast<T const>(source),
            std::const_pointer_cast<DigitalIntervalSeries const>(intervals));
}

#endif// GATHER_RESULT_HPP
