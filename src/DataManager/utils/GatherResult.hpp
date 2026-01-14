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

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/interval_data.hpp"

#include <algorithm>
#include <concepts>
#include <iterator>
#include <memory>
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

private:
    std::shared_ptr<T const> _source;
    std::shared_ptr<DigitalIntervalSeries const> _intervals;
    std::vector<value_type> _views;
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
