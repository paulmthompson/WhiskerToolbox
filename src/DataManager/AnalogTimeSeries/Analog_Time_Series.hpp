#ifndef ANALOG_TIME_SERIES_HPP
#define ANALOG_TIME_SERIES_HPP

#include "datamanager_export.h"
#include "Observer/Observer_Data.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"
#include "TypeTraits/DataTypeTraits.hpp"
#include "storage/AnalogDataStorage.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <vector>

/**
 * @brief The AnalogTimeSeries class
 *
 * Analog time series is used for storing continuous data
 * The data may be sampled at irregular intervals as long as the time vector is provided
 *
 */
class DATAMANAGER_EXPORT AnalogTimeSeries : public ObserverData {
public:
    // ========== Type Traits ==========
    /**
     * @brief Type traits for AnalogTimeSeries
     * 
     * Defines compile-time properties of AnalogTimeSeries for use in generic algorithms
     * and the transformation system.
     */
    struct DataTraits : WhiskerToolbox::TypeTraits::DataTypeTraitsBase<AnalogTimeSeries, float> {
        static constexpr bool is_ragged = false;
        static constexpr bool is_temporal = true;
        static constexpr bool has_entity_ids = false;
        static constexpr bool is_spatial = false;
    };

    // ========== Constructors ==========
    /**
     * @brief Default constructor
     * 
     * This constructor creates an empty AnalogTimeSeries with no data
     */
    AnalogTimeSeries();

    /**
     * @brief Constructor for AnalogTimeSeries from a vector of floats and a vector of TimeFrameIndex values
     * 
     * Use this constructor when the data is sampled at irregular intervals
     * 
     * @param analog_vector Vector of floats
     * @param time_vector Vector of TimeFrameIndex values
     * @see AnalogTimeSeries(std::vector<float> analog_vector, size_t num_samples) 
     * for a constructor that takes a vector of floats that are consecutive samples
     * @see AnalogTimeSeries(std::map<int, float> analog_map) 
     * for a constructor that takes a map of int to float
     */
    explicit AnalogTimeSeries(std::vector<float> analog_vector, std::vector<TimeFrameIndex> time_vector);

    /**
     * @brief Constructor for AnalogTimeSeries from a map of int to float
     * 
     * The key in the map is assumed to the be TimeFrameIndex for each sample
     * 
     * @param analog_map Map of int to float
     * @see AnalogTimeSeries(std::vector<float> analog_vector, std::vector<TimeFrameIndex> time_vector) 
     * for a constructor that takes a vector of floats and a vector of TimeFrameIndex values
     * @see AnalogTimeSeries(std::vector<float> analog_vector, size_t num_samples) 
     * for a constructor that takes a vector of floats that are consecutive samples
     */
    explicit AnalogTimeSeries(std::map<int, float> analog_map);

    /**
     * @brief Constructor for AnalogTimeSeries from a vector of floats and a number of samples
     * 
     * Use this constructor when the data is sampled at regular intervals increasing by 1
     * 
     * @param analog_vector Vector of floats
     * @param num_samples Number of samples
     * @see AnalogTimeSeries(std::vector<float> analog_vector, std::vector<TimeFrameIndex> time_vector) 
     * for constructors that support irregular sampling
     */
    explicit AnalogTimeSeries(std::vector<float> analog_vector, size_t num_samples);

    /**
     * @brief Construct from a range of time-value pairs
     * 
     * Accepts any input range that yields pairs of (TimeFrameIndex, float).
     * This enables efficient single-pass construction from transformed views.
     * 
     * @tparam R Range type (must satisfy std::ranges::input_range)
     * @param time_value_pairs Range of (TimeFrameIndex, float) pairs
     * 
     * @example
     * ```cpp
     * auto transformed = ragged_series 
     *     | std::views::transform([](auto entry) { 
     *         float sum = std::accumulate(entry.values.begin(), entry.values.end(), 0.0f);
     *         return std::pair{entry.time, sum};
     *     });
     * AnalogTimeSeries analog(transformed);
     * ```
     */
    template<std::ranges::input_range R>
        requires requires(std::ranges::range_value_t<R> pair) {
            { pair.first } -> std::convertible_to<TimeFrameIndex>;
            { pair.second } -> std::convertible_to<float>;
        }
    explicit AnalogTimeSeries(R && time_value_pairs)
        : AnalogTimeSeries() {

        // First pass: collect into vectors for efficient construction
        std::vector<TimeFrameIndex> times;
        std::vector<float> values;

        // Reserve if we can get size
        if constexpr (std::ranges::sized_range<R>) {
            auto size = std::ranges::size(time_value_pairs);
            times.reserve(size);
            values.reserve(size);
        }

        for (auto && [time, value]: time_value_pairs) {
            times.push_back(time);
            values.push_back(static_cast<float>(value));
        }

        // Use existing setData method for efficient storage setup
        setData(std::move(values), std::move(times));
    }

    // ========== Factory Methods ==========

    /**
     * @brief Create memory-mapped AnalogTimeSeries from binary file
     * 
     * Creates an AnalogTimeSeries that reads data from a binary file using memory mapping.
     * This is efficient for large datasets as it doesn't load the entire file into memory.
     * Supports strided access (e.g., reading one channel from multi-channel interleaved data).
     * 
     * @param config Memory-mapped storage configuration
     * @param time_vector Vector of TimeFrameIndex values corresponding to the samples
     * @return std::shared_ptr<AnalogTimeSeries> Memory-mapped analog time series
     * 
     * @throws std::runtime_error if file cannot be opened or configuration is invalid
     * 
     * @example Reading channel 5 from 384-channel int16 data:
     * @code
     * MmapStorageConfig config;
     * config.file_path = "ephys_data.bin";
     * config.header_size = 0;
     * config.offset = 5;  // Start at channel 5
     * config.stride = 384;  // Skip 384 values between samples
     * config.data_type = MmapDataType::Int16;
     * config.scale_factor = 0.195f;  // Convert to microvolts
     * config.num_samples = 0;  // Auto-detect
     * 
     * auto time_indices = createTimeVector(num_samples);
     * auto series = AnalogTimeSeries::createMemoryMapped(config, time_indices);
     * @endcode
     */
    [[nodiscard]] static std::shared_ptr<AnalogTimeSeries> createMemoryMapped(
            MmapStorageConfig config,
            std::vector<TimeFrameIndex> time_vector);

    /**
     * @brief Create AnalogTimeSeries from a lazy view
     * 
     * Creates an AnalogTimeSeries that computes values on-demand from a ranges view.
     * The view must be a random-access range that yields elements compatible with 
     * (TimeFrameIndex, float) pairs or TimeValuePoint. No intermediate data is 
     * materialized - values are computed when accessed.
     * 
     * This enables efficient transform pipelines without intermediate allocations:
     * @code
     * // Z-score normalization without materializing intermediate results
     * auto base_series = ;
     * float mean =;
     * float std = ;
     * 
     * auto normalized_view = base_series->view()
     *     | std::views::transform([mean, std](auto tv) {
     *         float z_score = (tv.value() - mean) / std;
     *         return AnalogTimeSeries::TimeValuePoint{tv.time_frame_index, z_score};
     *     });
     * 
     * auto normalized_series = AnalogTimeSeries::createFromView(
     *     normalized_view, 
     *     base_series->getTimeStorage()
     * );
     * 
     * // Access computes z-score on-demand
     * float value = normalized_series->getAtTime(TimeFrameIndex(100)).value();
     * @endcode
     * 
     * @tparam ViewType Random-access range type
     * @param view Random-access range view yielding (TimeFrameIndex, float) pairs or TimeValuePoint
     * @param time_storage Shared time index storage (reuse from base series for efficiency)
     * @return std::shared_ptr<AnalogTimeSeries> Lazy analog time series
     * 
     * @throws std::runtime_error if view size doesn't match time storage size
     * 
     * @note The view must remain valid for the lifetime of the returned AnalogTimeSeries.
     *       Capture by value in lambdas or ensure base series outlives the lazy series.
     * @note For materialized (eager) storage, call .materialize() on the result
     * 
     * @see materialize() to convert lazy storage to vector storage
     * @see view() to get a view over an existing AnalogTimeSeries
     */
    template<std::ranges::random_access_range ViewType>
    [[nodiscard]] static std::shared_ptr<AnalogTimeSeries> createFromView(
            ViewType view,
            std::shared_ptr<TimeIndexStorage> time_storage) {
        size_t num_samples = std::ranges::size(view);

        // Validate that view size matches time storage
        if (num_samples != time_storage->size()) {
            throw std::runtime_error(
                    "View size (" + std::to_string(num_samples) +
                    ") does not match time storage size (" +
                    std::to_string(time_storage->size()) + ")");
        }

        // Create lazy storage
        auto lazy_storage = LazyViewStorage<ViewType>(std::move(view), num_samples);
        DataStorageWrapper storage_wrapper(std::move(lazy_storage));

        // Use private constructor with shared time storage
        return std::shared_ptr<AnalogTimeSeries>(
                new AnalogTimeSeries(std::move(storage_wrapper), std::move(time_storage)));
    }

    /**
     * @brief Create a zero-copy view of an AnalogTimeSeries within a time range
     * 
     * Creates an AnalogTimeSeries that references the source's data without copying.
     * The view shares ownership of the underlying storage via shared_ptr, ensuring
     * the source data remains valid for the lifetime of the view.
     * 
     * This enables efficient windowed access patterns (e.g., plotting segments of
     * a long time series) without data duplication.
     * 
     * @param source Source AnalogTimeSeries to create a view from
     * @param start_time Start of the time range (inclusive)
     * @param end_time End of the time range (inclusive)
     * @return std::shared_ptr<AnalogTimeSeries> View into the source data
     * 
     * @throws std::runtime_error if source storage is not vector-backed (mmap or lazy)
     * 
     * @note The source must outlive all views or must be kept alive via shared_ptr.
     *       Since views share ownership of the underlying data, the data remains valid
     *       as long as any view exists.
     * @note View storage is contiguous (supports span access) since it references
     *       contiguous source data.
     * @note For lazy or mmap storage, call materialize() first then create views.
     * 
     * @example Creating windowed views for multi-trial plotting:
     * @code
     * auto full_series = std::make_shared<AnalogTimeSeries>(data, times);
     * 
     * std::vector<std::shared_ptr<AnalogTimeSeries>> trial_views;
     * for (auto const& trial_start : trial_times) {
     *     auto view = AnalogTimeSeries::createView(
     *         full_series,
     *         trial_start,
     *         TimeFrameIndex{trial_start.getValue() + window_size});
     *     trial_views.push_back(view);
     * }
     * 
     * // All views share the same underlying data (zero copies)
     * for (auto const& view : trial_views) {
     *     plotToBuffer(view->getSpan());
     * }
     * @endcode
     */
    [[nodiscard]] static std::shared_ptr<AnalogTimeSeries> createView(
            std::shared_ptr<AnalogTimeSeries const> source,
            TimeFrameIndex start_time,
            TimeFrameIndex end_time);

    // ========== Getting Data ==========

    [[nodiscard]] size_t getNumSamples() const { return _data_storage.size(); };

    /**
     * @brief Get a span over the analog data values
     * 
     * Returns a span providing a view over the analog time series data values.
     * This method provides efficient read-only access to contiguous data without copying.
     * For non-contiguous storage (e.g., memory-mapped with stride), returns empty span.
     * 
     * @return std::span<float const> view over the analog data values, or empty span if not contiguous
     * 
     * @note For contiguous storage (vector), this provides zero-copy access.
     *       For non-contiguous storage, use getAllSamples() iterator instead.
     * 
     * @see getTimeSeries() for accessing the corresponding time indices
     * @see getAllSamples() for time-value pair iteration that works with any storage
     */
    [[nodiscard]] std::span<float const> getAnalogTimeSeries() const { return _data_storage.getSpan(); };

    /**
     * @brief Get a span (view) of data values within a TimeFrameIndex range
     * 
     * This function returns a std::span over the data array for all points where 
     * TimeFrameIndex >= start_time and TimeFrameIndex <= end_time. The span provides
     * a zero-copy view into the underlying data.
     * 
     * If the exact start_time or end_time don't exist in the series, it finds the closest available times:
     * - For start: finds the smallest TimeFrameIndex >= start_time  
     * - For end: finds the largest TimeFrameIndex <= end_time
     * 
     * @param start_time The start time (inclusive boundary)
     * @param end_time The end time (inclusive boundary)
     * @return std::span<const float> view over the data in the specified range
     * 
     * @note Returns an empty span if no data points fall within the specified range
     * @note The span is valid as long as the AnalogTimeSeries object exists and is not modified
     * @see findDataArrayIndexGreaterOrEqual() and findDataArrayIndexLessOrEqual() for the underlying boundary logic
     */
    [[nodiscard]] std::span<float const> getDataInTimeFrameIndexRange(TimeFrameIndex start_time, TimeFrameIndex end_time) const;

    /**
     * @brief Get a span (view) of data values within a TimeFrameIndex range, with timeframe conversion
     *
     * This function returns a std::span over the data array for all points where
     * TimeFrameIndex >= start_time and TimeFrameIndex <= end_time. The span provides
     * a zero-copy view into the underlying data.
     *
     * If the exact start_time or end_time don't exist in the series, it finds the closest available times:
     * - For start: finds the smallest TimeFrameIndex >= start_time
     * - For end: finds the largest TimeFrameIndex <= end_time
     *
     * @param start_time The start time (inclusive boundary)
     * @param end_time The end time (inclusive boundary)
     * @param source_timeFrame The timeframe that the start and end times are expressed in
     * @return std::span<const float> view over the data in the specified range
     *
     * @note Returns an empty span if no data points fall within the specified range
     * @note The span is valid as long as the AnalogTimeSeries object exists and is not modified
     * @see findDataArrayIndexGreaterOrEqual() and findDataArrayIndexLessOrEqual() for the underlying boundary logic
     */
    [[nodiscard]] std::span<float const> getDataInTimeFrameIndexRange(TimeFrameIndex start_time,
                                                                      TimeFrameIndex end_time,
                                                                      TimeFrame const * source_timeFrame) const;

    // ========== Time-Value Range Access ==========

    /**
     * @brief Data structure representing a single time-value point
     * 
     * Represents a single sample in an AnalogTimeSeries.
     * Satisfies the TimeSeriesElement and ValueElement<float> concepts.
     * Does NOT satisfy EntityElement (AnalogTimeSeries has no EntityIds).
     * 
     * @see TimeSeriesConcepts.hpp for concept definitions
     */
    struct TimeValuePoint {
        TimeFrameIndex time_frame_index{TimeFrameIndex(0)};// Public for backward compatibility
        float _value{0.0f};                                // Prefixed to avoid collision with value() method

        TimeValuePoint() = default;
        TimeValuePoint(TimeFrameIndex time_idx, float val)
            : time_frame_index(time_idx),
              _value(val) {}

        // ========== Standardized Accessors (for TimeSeriesElement/ValueElement concepts) ==========

        /**
         * @brief Get the time of this sample (for TimeSeriesElement concept)
         * @return TimeFrameIndex The sample timestamp
         */
        [[nodiscard]] constexpr TimeFrameIndex time() const noexcept { return time_frame_index; }

        /**
         * @brief Get the value of this sample (for ValueElement concept)
         * @return float The sample value
         */
        [[nodiscard]] constexpr float value() const noexcept { return _value; }
    };

    /**
     * @brief Get a std::ranges compatible view of all samples as TimeValuePoint.
     * 
     * Returns a random-access view that synthesizes TimeValuePoint objects on demand.
     * Uses cached pointers for fast-path iteration when storage is contiguous.
     */
    [[nodiscard]] auto view() const {
        return std::views::iota(size_t{0}, getNumSamples()) | std::views::transform([this](size_t i) {
                   return TimeValuePoint{
                           _getTimeFrameIndexAtDataArrayIndex(DataArrayIndex(i)),
                           _getDataAtDataArrayIndex(DataArrayIndex(i))};
               });
    }

    /**
     * @brief Get a std::ranges compatible view of just the values (no time info).
     * 
     * Returns a random-access view of float values only, without time frame indices.
     */
    [[nodiscard]] auto viewValues() const {
        return std::views::iota(size_t{0}, getNumSamples()) | std::views::transform([this](size_t i) {
                   return _getDataAtDataArrayIndex(DataArrayIndex(i));
               });
    }

    /**
     * @brief Get time-value pairs in a TimeFrameIndex range as a view.
     * 
     * Returns a view over the time-value pairs within the specified range.
     * Uses internal index lookup to find the range boundaries.
     * 
     * @param start_index Starting DataArrayIndex
     * @param end_index Ending DataArrayIndex (exclusive)
     */
    [[nodiscard]] auto viewTimeValueRange(DataArrayIndex start_index, DataArrayIndex end_index) const {
        return std::views::iota(start_index.getValue(), end_index.getValue()) | std::views::transform([this](size_t i) {
                   return TimeValuePoint{
                           _getTimeFrameIndexAtDataArrayIndex(DataArrayIndex(i)),
                           _getDataAtDataArrayIndex(DataArrayIndex(i))};
               });
    }

    /**
     * @brief Time index range abstraction that handles both dense and sparse storage
     * 
     * Uses TimeIndexIterator from TimeIndexStorage for iteration
     */
    class TimeIndexRange {
    public:
        TimeIndexRange(AnalogTimeSeries const * series, DataArrayIndex start_index, DataArrayIndex end_index);

        [[nodiscard]] std::unique_ptr<::TimeIndexIterator> begin() const;
        [[nodiscard]] std::unique_ptr<::TimeIndexIterator> end() const;
        [[nodiscard]] size_t size() const;
        [[nodiscard]] bool empty() const;

    private:
        AnalogTimeSeries const * _series;
        DataArrayIndex _start_index;
        DataArrayIndex _end_index;
    };

    /**
     * @brief Paired span and time iterator for zero-copy time-value access
     */
    struct TimeValueSpanPair {
        std::span<float const> values;
        TimeIndexRange time_indices;

        TimeValueSpanPair(std::span<float const> data_span, AnalogTimeSeries const * series, DataArrayIndex start_index, DataArrayIndex end_index);
    };

    /**
     * @brief Get time-value pairs as a range for convenient iteration
     * 
     * Returns a range view that can be used with range-based for loops to iterate over
     * time-value pairs within the specified TimeFrameIndex range. This is the high-level,
     * convenient interface.
     * 
     * @param start_time The start TimeFrameIndex (inclusive boundary)
     * @param end_time The end TimeFrameIndex (inclusive boundary)
     * @return Range view that supports range-based for loops
     * 
     * @note Uses the same boundary logic as getDataInTimeFrameIndexRange()
     * @see getTimeValueSpanInTimeFrameIndexRange() for zero-copy alternative
     */
    [[nodiscard]] inline auto getTimeValueRangeInTimeFrameIndexRange(TimeFrameIndex start_time, TimeFrameIndex end_time) const {
        // Use existing boundary-finding logic
        auto start_index_opt = _findDataArrayIndexGreaterOrEqual(start_time);
        auto end_index_opt = _findDataArrayIndexLessOrEqual(end_time);

        // Handle cases where boundaries are not found
        if (!start_index_opt.has_value() || !end_index_opt.has_value()) {
            // Return empty range
            return viewTimeValueRange(DataArrayIndex(0), DataArrayIndex(0));
        }

        size_t start_idx = start_index_opt.value().getValue();
        size_t end_idx = end_index_opt.value().getValue();

        // Validate that start <= end
        if (start_idx > end_idx) {
            // Return empty range for invalid range
            return viewTimeValueRange(DataArrayIndex(0), DataArrayIndex(0));
        }

        // Create range view (end_idx + 1 because end is exclusive for the range)
        return viewTimeValueRange(DataArrayIndex(start_idx), DataArrayIndex(end_idx + 1));
    }

    /**
     * @brief Get time-value pairs as a range with timeframe conversion
     * 
     * Similar to getTimeValueRangeInTimeFrameIndexRange, but accepts a source timeframe
     * to convert the start and end time indices from the source coordinate system to
     * the analog time series coordinate system.
     * 
     * @param start_time The start TimeFrameIndex in source timeframe coordinates
     * @param end_time The end TimeFrameIndex in source timeframe coordinates
     * @param source_timeFrame The timeframe that start_time and end_time are expressed in
     * @return Range view that supports range-based for loops
     * 
     * @note If source_timeFrame equals the analog series' timeframe, or if either is null,
     *       falls back to the non-converting version
     */
    [[nodiscard]] inline auto getTimeValueRangeInTimeFrameIndexRange(TimeFrameIndex start_time,
                                                                     TimeFrameIndex end_time,
                                                                     TimeFrame const & source_timeFrame) const {
        // If source timeframe is the same as our timeframe, no conversion needed
        if (&source_timeFrame == _time_frame.get()) {
            return getTimeValueRangeInTimeFrameIndexRange(start_time, end_time);
        }

        // If we don't have a timeframe, fall back to non-converting version
        if (!_time_frame) {
            return getTimeValueRangeInTimeFrameIndexRange(start_time, end_time);
        }

        // Convert the time indices from source timeframe to our timeframe
        auto [target_start, target_end] = convertTimeFrameRange(
                start_time, end_time, source_timeFrame, *_time_frame);

        return getTimeValueRangeInTimeFrameIndexRange(target_start, target_end);
    }

    /**
     * @brief Get time-value pairs as span and time iterator for zero-copy access
     * 
     * Returns a structure containing a zero-copy span over the data values and a
     * time iterator for the corresponding TimeFrameIndex values. This is the 
     * performance-optimized interface.
     * 
     * @param start_time The start TimeFrameIndex (inclusive boundary)
     * @param end_time The end TimeFrameIndex (inclusive boundary)
     * @return TimeValueSpanPair with zero-copy data access
     * 
     * @note Uses the same boundary logic as getDataInTimeFrameIndexRange()
     * @see getTimeValueRangeInTimeFrameIndexRange() for convenient range-based alternative
     */
    [[nodiscard]] TimeValueSpanPair getTimeValueSpanInTimeFrameIndexRange(TimeFrameIndex start_time,
                                                                          TimeFrameIndex end_time) const;

    /**
     * @brief Get time-value pairs with timeframe conversion
     * 
     * Similar to getTimeValueSpanInTimeFrameIndexRange, but accepts a source timeframe
     * to convert the start and end time indices from the source coordinate system to
     * the analog time series coordinate system.
     * 
     * @param start_time The start TimeFrameIndex in source timeframe coordinates
     * @param end_time The end TimeFrameIndex in source timeframe coordinates
     * @param source_timeFrame The timeframe that start_time and end_time are expressed in
     * @return TimeValueSpanPair with zero-copy data access
     * 
     * @note If source_timeFrame equals the analog series' timeframe, or if either is null,
     *       falls back to the non-converting version
     */
    [[nodiscard]] TimeValueSpanPair getTimeValueSpanInTimeFrameIndexRange(TimeFrameIndex start_time,
                                                                          TimeFrameIndex end_time,
                                                                          TimeFrame const * source_timeFrame) const;

    /**
     * @brief Get all samples as time-value pairs for range-based iteration
     * 
     * Returns a range view over all samples in the time series, providing paired
     * TimeFrameIndex and float values. This is the recommended interface for iterating
     * over all data as it works with any storage backend (vector, memory-mapped, etc.).
     * 
     * @return Range view that supports range-based for loops
     * 
     * @example
     * ```cpp
     * for (auto const& sample : analog_series->getAllSamples()) {
     *     std::cout << sample.time_frame_index.getValue() << ": " << sample.value << std::endl;
     * }
     * ```
     * 
     * @note This provides a uniform interface regardless of underlying storage type
     * @see TimeValuePoint for the structure returned by dereferencing the iterator
     */
    [[nodiscard]] auto getAllSamples() const { return view(); }

    /**
     * @brief Get the time indices as a vector
     * 
     * Returns a vector containing the time indices corresponding to each analog data sample.
     * For dense time storage, this generates the indices on-demand.
     * For sparse time storage, this returns a copy of the stored indices.
     * 
     * @return std::vector<size_t> containing the time indices
     * 
     * @note For dense storage, this method has O(n) time complexity as it generates the vector.
     *       Consider using getTimeAtIndex() for single lookups or iterating with getNumSamples().
     *       For sparse storage, this returns a copy of the internal vector.
     * 
     * @see getAnalogTimeSeries() for accessing the corresponding data values
     * @see getDataInRange() for accessing time-value pairs within a specific range
     * @see getTimeAtIndex() for single index lookups
     */
    [[nodiscard]] std::vector<TimeFrameIndex> getTimeSeries() const {
        return _time_storage->getAllTimeIndices();
    }

    // ========== Time Storage Access ==========

    /**
    * @brief Get a const reference to the time index storage
    *
    * This allows efficient access to the underlying time mapping without copying.
    *
    * @return const shared_ptr to TimeIndexStorage
    */
    [[nodiscard]] std::shared_ptr<TimeIndexStorage> const & getTimeStorage() const noexcept { return _time_storage; }

    // ========== Time Frame ==========
    /**
     * @brief Set the time frame
     * 
     * @param time_frame The time frame to set
     */
    void setTimeFrame(std::shared_ptr<TimeFrame> time_frame) { _time_frame = time_frame; }

    /**
     * @brief Get the time frame
     * 
     * @return std::shared_ptr<TimeFrame> The time frame
     */
    [[nodiscard]] std::shared_ptr<TimeFrame> getTimeFrame() const { return _time_frame; }

protected:
private:
    /**
     * @brief Type-erased wrapper for analog data storage
     * 
     * Provides uniform interface to different storage backends (vector, mmap, view, etc.)
     * while enabling compile-time optimizations through template instantiation.
     * 
     * Uses shared_ptr internally to enable zero-copy view creation via aliasing constructor.
     * When creating a view from a VectorAnalogDataStorage, the view can share ownership
     * of the source data without copying.
     */
    class DataStorageWrapper {
    public:
        template<typename DataStorageImpl>
        explicit DataStorageWrapper(DataStorageImpl storage)
            : _impl(std::make_shared<StorageModel<DataStorageImpl>>(std::move(storage))) {}

        // Default constructor creates empty vector storage
        DataStorageWrapper()
            : _impl(std::make_shared<StorageModel<VectorAnalogDataStorage>>(
                  VectorAnalogDataStorage{std::vector<float>{}})) {}

        // Copy and move semantics - shared_ptr allows sharing
        DataStorageWrapper(DataStorageWrapper&&) noexcept = default;
        DataStorageWrapper& operator=(DataStorageWrapper&&) noexcept = default;
        DataStorageWrapper(DataStorageWrapper const&) = default;
        DataStorageWrapper& operator=(DataStorageWrapper const&) = default;

        [[nodiscard]] size_t size() const { return _impl->size(); }

        [[nodiscard]] float getValueAt(size_t index) const {
            return _impl->getValueAt(index);
        }

        [[nodiscard]] std::span<float const> getSpan() const {
            return _impl->getSpan();
        }

        [[nodiscard]] std::span<float const> getSpanRange(size_t start, size_t end) const {
            return _impl->getSpanRange(start, end);
        }

        [[nodiscard]] bool isContiguous() const {
            return _impl->isContiguous();
        }

        [[nodiscard]] float const * tryGetContiguousPointer() const {
            return _impl->tryGetContiguousPointer();
        }

        [[nodiscard]] AnalogStorageType getStorageType() const {
            return _impl->getStorageType();
        }

        [[nodiscard]] bool isView() const {
            return getStorageType() == AnalogStorageType::View;
        }

        [[nodiscard]] bool isLazy() const {
            return getStorageType() == AnalogStorageType::LazyView;
        }

        /**
         * @brief Get shared pointer to vector storage for creating views
         * 
         * If this wrapper contains vector storage, uses aliasing constructor to share
         * ownership. If this wrapper contains a view, returns the view's existing source.
         * Returns nullptr for other storage types (mmap, lazy).
         * 
         * @return shared_ptr to VectorAnalogDataStorage, or nullptr if not available
         */
        [[nodiscard]] std::shared_ptr<VectorAnalogDataStorage const> getSharedVectorStorage() const {
            // Check if we have view storage - return its existing source
            if (auto const* view_model = dynamic_cast<StorageModel<ViewAnalogDataStorage> const*>(_impl.get())) {
                return view_model->_storage.source();
            }

            // Check if we have vector storage - use aliasing constructor for zero-copy sharing
            if (auto vector_model = std::dynamic_pointer_cast<StorageModel<VectorAnalogDataStorage> const>(_impl)) {
                // Aliasing constructor: shares ownership with _impl but points to the inner storage
                return std::shared_ptr<VectorAnalogDataStorage const>(vector_model, &vector_model->_storage);
            }

            // Other storage types (mmap, lazy) - no shared vector storage available
            return nullptr;
        }

    private:
        struct StorageConcept {
            virtual ~StorageConcept() = default;
            virtual size_t size() const = 0;
            virtual float getValueAt(size_t index) const = 0;
            virtual std::span<float const> getSpan() const = 0;
            virtual std::span<float const> getSpanRange(size_t start, size_t end) const = 0;
            virtual bool isContiguous() const = 0;
            virtual float const * tryGetContiguousPointer() const = 0;
            virtual AnalogStorageType getStorageType() const = 0;
        };

        template<typename DataStorageImpl>
        struct StorageModel : StorageConcept {
            DataStorageImpl _storage;

            explicit StorageModel(DataStorageImpl storage)
                : _storage(std::move(storage)) {}

            size_t size() const override {
                return _storage.size();
            }

            float getValueAt(size_t index) const override {
                return _storage.getValueAt(index);
            }

            std::span<float const> getSpan() const override {
                return _storage.getSpan();
            }

            std::span<float const> getSpanRange(size_t start, size_t end) const override {
                return _storage.getSpanRange(start, end);
            }

            bool isContiguous() const override {
                return _storage.isContiguous();
            }

            float const * tryGetContiguousPointer() const override {
                if constexpr (std::is_same_v<DataStorageImpl, VectorAnalogDataStorage>) {
                    return _storage.data();
                } else if constexpr (std::is_same_v<DataStorageImpl, ViewAnalogDataStorage>) {
                    return _storage.data();
                }
                return nullptr;
            }

            AnalogStorageType getStorageType() const override {
                return _storage.getStorageType();
            }
        };

        std::shared_ptr<StorageConcept> _impl;
    };

    DataStorageWrapper _data_storage;
    std::shared_ptr<TimeIndexStorage> _time_storage;
    std::shared_ptr<TimeFrame> _time_frame{nullptr};

    // Cached optimization pointer for fast path access
    float const * _contiguous_data_ptr{nullptr};

    // Private constructors for factory methods
    AnalogTimeSeries(DataStorageWrapper storage, std::vector<TimeFrameIndex> time_vector);

    // Constructor for reusing shared time storage (efficient for lazy views)
    AnalogTimeSeries(DataStorageWrapper storage, std::shared_ptr<TimeIndexStorage> time_storage)
        : _data_storage(std::move(storage)),
          _time_storage(std::move(time_storage)) {
        _cacheOptimizationPointers();
    }

    void setData(std::vector<float> analog_vector);
    void setData(std::vector<float> analog_vector, std::vector<TimeFrameIndex> time_vector);
    void setData(std::map<int, float> analog_map);

    /**
     * @brief Cache optimization pointers after construction
     * 
     * Attempts to extract direct pointer to contiguous data for fast path access.
     * Called in constructors and setData methods.
     */
    void _cacheOptimizationPointers() {
        _contiguous_data_ptr = _data_storage.tryGetContiguousPointer();
    }

    /**
     * @brief Get the data value at a specific DataArrayIndex (internal use only)
     * 
     * Uses fast path (cached pointer) when available, falls back to virtual dispatch.
     * 
     * @param i The DataArrayIndex to get the data value at
     * @return The data value at the specified DataArrayIndex
     */
    [[nodiscard]] float _getDataAtDataArrayIndex(DataArrayIndex i) const {
        if (_contiguous_data_ptr) {
            return _contiguous_data_ptr[i.getValue()];
        }
        return _data_storage.getValueAt(i.getValue());
    }

    /**
     * @brief Get the TimeFrameIndex that corresponds to a given DataArrayIndex (internal use only)
     * 
     * @param i The DataArrayIndex to get the TimeFrameIndex for
     * @return The TimeFrameIndex that corresponds to the given DataArrayIndex
     */
    [[nodiscard]] TimeFrameIndex _getTimeFrameIndexAtDataArrayIndex(DataArrayIndex i) const {
        return _time_storage->getTimeFrameIndexAt(i.getValue());
    }

    /**
     * @brief Find the DataArrayIndex that corresponds to a given TimeFrameIndex
     * 
     * This function searches for the DataArrayIndex position that corresponds to the given TimeFrameIndex.
     * For dense storage, it calculates the position if the TimeFrameIndex falls within the range.
     * For sparse storage, it searches for the TimeFrameIndex in the stored indices.
     * 
     * @param time_index The TimeFrameIndex to search for
     * @return std::optional<DataArrayIndex> containing the corresponding DataArrayIndex, or std::nullopt if not found
     */
    [[nodiscard]] std::optional<DataArrayIndex> _findDataArrayIndexForTimeFrameIndex(TimeFrameIndex time_index) const;

    /**
     * @brief Find the DataArrayIndex for the smallest TimeFrameIndex >= target_time
     * 
     * This function finds the first data point where TimeFrameIndex >= target_time.
     * Useful for finding the start boundary of a time range when the exact time may not exist.
     * 
     * @param target_time The target TimeFrameIndex
     * @return std::optional<DataArrayIndex> containing the DataArrayIndex of the first TimeFrameIndex >= target_time, or std::nullopt if no such index exists
     */
    [[nodiscard]] std::optional<DataArrayIndex> _findDataArrayIndexGreaterOrEqual(TimeFrameIndex target_time) const;

    /**
     * @brief Find the DataArrayIndex for the largest TimeFrameIndex <= target_time
     * 
     * This function finds the last data point where TimeFrameIndex <= target_time.
     * Useful for finding the end boundary of a time range when the exact time may not exist.
     * 
     * @param target_time The target TimeFrameIndex
     * @return std::optional<DataArrayIndex> containing the DataArrayIndex of the last TimeFrameIndex <= target_time, or std::nullopt if no such index exists
     */
    [[nodiscard]] std::optional<DataArrayIndex> _findDataArrayIndexLessOrEqual(TimeFrameIndex target_time) const;

public:
    // ========== Iteration & Access ==========

    /**
     * @brief Get a view of (TimeFrameIndex, float) pairs
     * 
     * Enables iterating over the time series as a sequence of (time, value) pairs.
     * Compatible with TransformPipeline.
     * 
     * Returns `std::pair<TimeFrameIndex, float>` for backward compatibility
     * with existing code that uses `.first`, `.second`, and structured bindings.
     * 
     * @return A lazy range view of (TimeFrameIndex, float) pairs
     * @see elementsView() for concept-compliant iteration with TimeValuePoint
     */
    [[nodiscard]] auto elements() const {
        return std::views::iota(size_t(0), _data_storage.size()) | std::views::transform([this](size_t i) {
                   return std::make_pair(
                           _time_storage->getTimeFrameIndexAt(i),
                           _data_storage.getValueAt(i));
               });
    }

    /**
     * @brief Get a view of TimeValuePoint objects (concept-compliant)
     * 
     * Enables iterating over the time series as a sequence of TimeValuePoint objects.
     * Each element satisfies the TimeSeriesElement and ValueElement<float> concepts,
     * enabling use with generic time series algorithms.
     * 
     * Use this method when you need concept-compliant elements for generic algorithms.
     * Use elements() when you need backward-compatible pair iteration.
     * 
     * @return A lazy range view of TimeValuePoint objects
     * @see TimeValuePoint
     * @see TimeSeriesConcepts.hpp for concept definitions
     */
    [[nodiscard]] auto elementsView() const {
        return std::views::iota(size_t(0), _data_storage.size()) | std::views::transform([this](size_t i) {
                   return TimeValuePoint{
                           _time_storage->getTimeFrameIndexAt(i),
                           _data_storage.getValueAt(i)};
               });
    }

    /**
     * @brief Get value at specific time
     * 
     * @param time The time to get value for
     * @return Optional float value if time exists
     */
    [[nodiscard]] std::optional<float> getAtTime(TimeFrameIndex time) const {
        auto idx = _findDataArrayIndexForTimeFrameIndex(time);
        if (idx) {
            return _getDataAtDataArrayIndex(*idx);
        }
        return std::nullopt;
    }

    // ========== Materialization ==========

    /**
     * @brief Materialize lazy storage into vector storage
     * 
     * Converts any storage backend (lazy, memory-mapped, etc.) into contiguous
     * vector storage by evaluating all values and copying them into memory.
     * Useful when:
     * - Random access patterns would cause repeated computation
     * - Downstream operations require contiguous memory
     * - Original data source (file, base series) will be destroyed
     * 
     * @return std::shared_ptr<AnalogTimeSeries> New series with vector storage
     * 
     * @note For already-materialized vector storage, this creates a deep copy
     * @note This will evaluate all lazy computations immediately
     * 
     * @example
     * @code
     * auto lazy_series = AnalogTimeSeries::createFromView(transformed_view, time_storage);
     * // ... do some sequential processing ...
     * 
     * // Materialize before random access-heavy operations
     * auto materialized = lazy_series->materialize();
     * 
     * // Now random access is efficient
     * for (int i = 0; i < 1000; ++i) {
     *     auto value = materialized->getAtTime(TimeFrameIndex(random_indices[i]));
     * }
     * @endcode
     */
    [[nodiscard]] std::shared_ptr<AnalogTimeSeries> materialize() const {
        // Fast path: already vector storage, just deep copy
        if (_data_storage.getStorageType() == AnalogStorageType::Vector) {
            std::vector<float> values;
            auto span = _data_storage.getSpan();
            values.assign(span.begin(), span.end());
            return std::make_shared<AnalogTimeSeries>(
                    std::move(values),
                    _time_storage->getAllTimeIndices());
        }

        // Slow path: evaluate all values from storage
        size_t n = _data_storage.size();
        std::vector<float> values;
        values.reserve(n);

        for (size_t i = 0; i < n; ++i) {
            values.push_back(_data_storage.getValueAt(i));
        }

        return std::make_shared<AnalogTimeSeries>(
                std::move(values),
                _time_storage->getAllTimeIndices());
    }
};

#endif// ANALOG_TIME_SERIES_HPP
