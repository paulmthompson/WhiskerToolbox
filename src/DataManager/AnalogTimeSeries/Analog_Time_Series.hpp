#ifndef ANALOG_TIME_SERIES_HPP
#define ANALOG_TIME_SERIES_HPP

#include "AnalogDataStorage.hpp"
#include "Observer/Observer_Data.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"
#include "TypeTraits/DataTypeTraits.hpp"

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
class AnalogTimeSeries : public ObserverData {
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
    explicit AnalogTimeSeries(R&& time_value_pairs) 
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
        
        for (auto&& [time, value] : time_value_pairs) {
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
     */
    struct TimeValuePoint {
        TimeFrameIndex time_frame_index{TimeFrameIndex(0)};
        float value{0.0f};

        TimeValuePoint() = default;
        TimeValuePoint(TimeFrameIndex time_idx, float val)
            : time_frame_index(time_idx),
              value(val) {}
    };

    class TimeValueRangeIterator {
    public:
        // 1. Upgrade category to Random Access
        using iterator_category = std::random_access_iterator_tag;

        // 2. C++20 requires distinct iterator_concept for ranges
        using iterator_concept = std::random_access_iterator_tag;

        using value_type = TimeValuePoint;
        using difference_type = std::ptrdiff_t;
        using pointer = TimeValuePoint;  // Proxy pointer (optional, or strictly TimeValuePoint*)
        using reference = TimeValuePoint;// RETURN BY VALUE

        // 3. Must be default constructible
        TimeValueRangeIterator() = default;

        TimeValueRangeIterator(AnalogTimeSeries const * series, DataArrayIndex current, DataArrayIndex end)
            : _series(series),
              _current_index(current),
              _end_index(end) {
            // Cache the contiguous pointer if available for speed
            if (_series) _contiguous_data_ptr = _series->_data_storage.tryGetContiguousPointer();
        }

        // Dereference returns by Value (Cleanest for stashing iterators)
        reference operator*() const {
            // Use fast path if available
            float val = (_contiguous_data_ptr)
                                ? _contiguous_data_ptr[_current_index.getValue()]
                                : _series->_data_storage.getValueAt(_current_index.getValue());

            // Assume _time_storage has a similar fast lookup, otherwise use existing accessor
            TimeFrameIndex time = _series->_getTimeFrameIndexAtDataArrayIndex(_current_index);

            return TimeValuePoint{time, val};
        }

        // Standard Iterator Operations
        TimeValueRangeIterator & operator++() {
            // Assuming DataArrayIndex has operator++
            // _current_index++;
            // If not, generic increment:
            _current_index = DataArrayIndex(_current_index.getValue() + 1);
            return *this;
        }

        TimeValueRangeIterator operator++(int) {
            TimeValueRangeIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        TimeValueRangeIterator & operator--() {
            _current_index = DataArrayIndex(_current_index.getValue() - 1);
            return *this;
        }

        TimeValueRangeIterator operator--(int) {
            TimeValueRangeIterator tmp = *this;
            --(*this);
            return tmp;
        }

        // Random Access Arithmetic
        TimeValueRangeIterator & operator+=(difference_type n) {
            _current_index = DataArrayIndex(_current_index.getValue() + n);
            return *this;
        }

        TimeValueRangeIterator & operator-=(difference_type n) {
            _current_index = DataArrayIndex(_current_index.getValue() - n);
            return *this;
        }

        friend TimeValueRangeIterator operator+(TimeValueRangeIterator it, difference_type n) { return it += n; }
        friend TimeValueRangeIterator operator+(difference_type n, TimeValueRangeIterator it) { return it += n; }
        friend TimeValueRangeIterator operator-(TimeValueRangeIterator it, difference_type n) { return it -= n; }

        friend difference_type operator-(TimeValueRangeIterator const & lhs, TimeValueRangeIterator const & rhs) {
            return static_cast<difference_type>(lhs._current_index.getValue()) -
                   static_cast<difference_type>(rhs._current_index.getValue());
        }

        // Comparisons
        bool operator==(TimeValueRangeIterator const & other) const {
            return _current_index.getValue() == other._current_index.getValue();
        }

        // Default C++20 spaceship operator handles !=, <, >, <=, >= automatically
        auto operator<=>(TimeValueRangeIterator const & other) const {
            return _current_index.getValue() <=> other._current_index.getValue();
        }

        // Random access subscription
        reference operator[](difference_type n) const {
            return *(*this + n);
        }

    private:
        AnalogTimeSeries const * _series = nullptr;
        DataArrayIndex _current_index{0};
        DataArrayIndex _end_index{0};
        float const * _contiguous_data_ptr{nullptr};
    };

    class TimeValueRangeView : public std::ranges::view_interface<TimeValueRangeView> {
    public:
        // 1. Must be default constructible
        TimeValueRangeView() = default;

        TimeValueRangeView(AnalogTimeSeries const * series, DataArrayIndex start, DataArrayIndex end)
            : _series(series),
              _start_index(start),
              _end_index(end) {}

        [[nodiscard]] TimeValueRangeIterator begin() const {
            return TimeValueRangeIterator(_series, _start_index, _end_index);
        }

        [[nodiscard]] TimeValueRangeIterator end() const {
            return TimeValueRangeIterator(_series, _end_index, _end_index);
        }

        // size() is actually provided by view_interface if iterator is RandomAccess and sized
        // But explicit implementation is often faster/safer if you have the data
        [[nodiscard]] size_t size() const {
            return _end_index.getValue() - _start_index.getValue();
        }

    private:
        AnalogTimeSeries const * _series = nullptr;
        DataArrayIndex _start_index{0};
        DataArrayIndex _end_index{0};
    };

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
     * @return TimeValueRangeView that supports range-based for loops
     * 
     * @note Uses the same boundary logic as getDataInTimeFrameIndexRange()
     * @see getTimeValueSpanInTimeFrameIndexRange() for zero-copy alternative
     */
    [[nodiscard]] TimeValueRangeView getTimeValueRangeInTimeFrameIndexRange(TimeFrameIndex start_time, TimeFrameIndex end_time) const;

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
    [[nodiscard]] TimeValueSpanPair getTimeValueSpanInTimeFrameIndexRange(TimeFrameIndex start_time, TimeFrameIndex end_time) const;

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
     * @return TimeValueRangeView that supports range-based for loops
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
    [[nodiscard]] TimeValueRangeView getAllSamples() const;

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
 * @brief Get a ranges-compatible view of the entire series.
 */
    [[nodiscard]] auto view() const {
        // Assuming DataArrayIndex can be constructed from size_t 0 and size()
        return TimeValueRangeView(this, DataArrayIndex(0), DataArrayIndex(getNumSamples()));
    }

protected:
private:
    /**
     * @brief Type-erased wrapper for analog data storage
     * 
     * Provides uniform interface to different storage backends (vector, mmap, etc.)
     * while enabling compile-time optimizations through template instantiation.
     */
    class DataStorageWrapper {
    public:
        template<typename DataStorageImpl>
        explicit DataStorageWrapper(DataStorageImpl storage)
            : _impl(std::make_unique<StorageModel<DataStorageImpl>>(std::move(storage))) {}

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
                }
                return nullptr;
            }

            AnalogStorageType getStorageType() const override {
                return _storage.getStorageType();
            }
        };

        std::unique_ptr<StorageConcept> _impl;
    };

    DataStorageWrapper _data_storage;
    std::shared_ptr<TimeIndexStorage> _time_storage;
    std::shared_ptr<TimeFrame> _time_frame{nullptr};

    // Cached optimization pointer for fast path access
    float const * _contiguous_data_ptr{nullptr};

    // Private constructor for factory methods
    AnalogTimeSeries(DataStorageWrapper storage, std::vector<TimeFrameIndex> time_vector);

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
};

#endif// ANALOG_TIME_SERIES_HPP