#ifndef ANALOG_TIME_SERIES_HPP
#define ANALOG_TIME_SERIES_HPP

#include "Observer/Observer_Data.hpp"
#include "TimeFrame/TimeFrameV2.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <ranges>
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
    AnalogTimeSeries() = default;
    explicit AnalogTimeSeries(std::vector<float> analog_vector);
    AnalogTimeSeries(std::vector<float> analog_vector, std::vector<size_t> time_vector);
    explicit AnalogTimeSeries(std::map<int, float> analog_map);

    void setData(std::vector<float> analog_vector);

    void setData(std::vector<float> analog_vector, std::vector<size_t> time_vector);

    void setData(std::map<int, float> analog_map);

    void overwriteAtTimes(std::vector<float> & analog_data, std::vector<size_t> & time);

    [[nodiscard]] float getDataAtIndex(size_t i) const { return _data[i]; };
    [[nodiscard]] size_t getTimeAtIndex(size_t i) const { return _time[i]; };

    [[nodiscard]] size_t getNumSamples() const { return _data.size(); };

    /**
     * @brief Get a const reference to the analog data vector
     * 
     * Returns a const reference to the internal vector containing the analog time series data values.
     * This method provides efficient read-only access to the data without copying.
     * 
     * @return const reference to std::vector<float> containing the analog data values
     * 
     * @note This method returns by const reference for performance - no data copying occurs.
     *       Use this when you need to iterate over or access the raw data values efficiently.
     * 
     * @see getTimeSeries() for accessing the corresponding time indices
     * @see getDataInRange() for accessing data within a specific time range
     */
    [[nodiscard]] std::vector<float> const & getAnalogTimeSeries() const { return _data; };

    /**
     * @brief Get a const reference to the time indices vector
     * 
     * Returns a const reference to the internal vector containing the time indices corresponding
     * to each analog data sample. Each element represents the time index for the corresponding
     * data value in the analog time series.
     * 
     * @return const reference to std::vector<size_t> containing the time indices
     * 
     * @note This method returns by const reference for performance - no data copying occurs.
     *       The time indices correspond one-to-one with the analog data values.
     *       Use this when you need efficient access to the timing information.
     * 
     * @see getAnalogTimeSeries() for accessing the corresponding data values
     * @see getDataInRange() for accessing time-value pairs within a specific range
     */
    [[nodiscard]] std::vector<size_t> const & getTimeSeries() const { return _time; };

    auto getDataInRange(float start_time, float stop_time) const {
        struct DataPoint {
            size_t time_idx;
            float value;
        };

        return std::views::iota(size_t{0}, getNumSamples()) |
               std::views::filter([this, start_time, stop_time](size_t i) {
                   auto transformed_time = static_cast<float>(getTimeAtIndex(i));
                   return transformed_time >= start_time && transformed_time <= stop_time;
               }) |
               std::views::transform([this](size_t i) {
                   return DataPoint{getTimeAtIndex(i), getDataAtIndex(i)};
               });
    }

    template<typename TransformFunc>
    auto getDataInRange(float start_time, float stop_time, TransformFunc time_transform) const {
        struct DataPoint {
            size_t time_idx;
            float value;
        };

        return std::views::iota(size_t{0}, getNumSamples()) |
               std::views::filter([this, start_time, stop_time, time_transform](size_t i) {
                   auto transformed_time = time_transform(getTimeAtIndex(i));
                   return transformed_time >= start_time && transformed_time <= stop_time;
               }) |
               std::views::transform([this](size_t i) {
                   return DataPoint{getTimeAtIndex(i), getDataAtIndex(i)};
               });
    }

    std::pair<std::vector<size_t>, std::vector<float>> getDataVectorsInRange(
            float start_time, float stop_time) const {
        std::vector<size_t> times;
        std::vector<float> values;

        auto range = getDataInRange(start_time, stop_time);
        for (auto const & point: range) {
            times.push_back(point.time_idx);
            values.push_back(point.value);
        }

        return {times, values};
    }

    template<typename TransformFunc>
    std::pair<std::vector<size_t>, std::vector<float>> getDataVectorsInRange(
            float start_time, float stop_time, TransformFunc time_transform) const {
        std::vector<size_t> times;
        std::vector<float> values;

        auto range = getDataInRange(start_time, stop_time, time_transform);
        for (auto const & point: range) {
            times.push_back(point.time_idx);
            values.push_back(point.value);
        }

        return {times, values};
    }

    // ========== TimeFrameV2 Support ==========

    /**
    * @brief Set a TimeFrameV2 reference for this data series
    *
    * Associates this AnalogTimeSeries with a strongly-typed TimeFrameV2,
    * enabling type-safe time coordinate operations.
    *
    * @param timeframe The TimeFrameV2 variant to associate with this data
    */
    void setTimeFrameV2(AnyTimeFrame timeframe) {
        _timeframe_v2 = std::move(timeframe);
    }

    /**
    * @brief Get the TimeFrameV2 reference for this data series
    *
    * @return Optional TimeFrameV2 variant if set, nullopt otherwise
    */
    [[nodiscard]] std::optional<AnyTimeFrame> getTimeFrameV2() const {
        return _timeframe_v2;
    }

    /**
    * @brief Check if this series has a TimeFrameV2 reference
    *
    * @return True if a TimeFrameV2 is associated with this series
    */
    [[nodiscard]] bool hasTimeFrameV2() const {
        return _timeframe_v2.has_value();
    }

    /**
    * @brief Get data values in a time range using TimeFrameV2 coordinates
    *
    * This method uses the associated TimeFrameV2 to perform type-safe
    * time coordinate queries. It works with any coordinate type.
    *
    * @tparam CoordinateType The time coordinate type (e.g., ClockTicks, CameraFrameIndex)
    * @param start_coord Start coordinate in the TimeFrameV2's coordinate system
    * @param end_coord End coordinate in the TimeFrameV2's coordinate system
    * @return Vector of data values within the specified coordinate range
    * @throws std::runtime_error if no TimeFrameV2 is associated or types don't match
    */
    template<typename CoordinateType>
    std::vector<float> getDataInCoordinateRange(CoordinateType start_coord, CoordinateType end_coord) const {
        if (!_timeframe_v2.has_value()) {
            throw std::runtime_error("No TimeFrameV2 associated with this AnalogTimeSeries");
        }

        return std::visit([&](auto const & timeframe_ptr) -> std::vector<float> {
            using FrameType = std::decay_t<decltype(*timeframe_ptr)>;
            using FrameCoordType = typename FrameType::coordinate_type;

            if constexpr (std::is_same_v<FrameCoordType, CoordinateType>) {
                std::vector<float> result;

                // Get the range of indices for the coordinate range
                int64_t start_idx = timeframe_ptr->getIndexAtTime(start_coord);
                int64_t end_idx = timeframe_ptr->getIndexAtTime(end_coord);

                if (start_idx > end_idx) std::swap(start_idx, end_idx);

                result.reserve(static_cast<size_t>(end_idx - start_idx + 1));

                for (int64_t idx = start_idx; idx <= end_idx; ++idx) {
                    if (idx >= 0 && static_cast<size_t>(idx) < _data.size()) {
                        result.push_back(_data[static_cast<size_t>(idx)]);
                    }
                }

                return result;
            } else {
                throw std::runtime_error("Coordinate type mismatch with TimeFrameV2");
            }
        },
                          _timeframe_v2.value());
    }

    /**
    * @brief Get data values and their coordinates in a time range
    *
    * Similar to getDataInCoordinateRange but returns both values and coordinates.
    *
    * @tparam CoordinateType The time coordinate type
    * @param start_coord Start coordinate
    * @param end_coord End coordinate
    * @return Pair of coordinate vector and value vector
    */
    template<typename CoordinateType>
    std::pair<std::vector<CoordinateType>, std::vector<float>> getDataAndCoordsInRange(
            CoordinateType start_coord, CoordinateType end_coord) const {
        if (!_timeframe_v2.has_value()) {
            throw std::runtime_error("No TimeFrameV2 associated with this AnalogTimeSeries");
        }

        return std::visit([&](auto const & timeframe_ptr) -> std::pair<std::vector<CoordinateType>, std::vector<float>> {
            using FrameType = std::decay_t<decltype(*timeframe_ptr)>;
            using FrameCoordType = typename FrameType::coordinate_type;

            if constexpr (std::is_same_v<FrameCoordType, CoordinateType>) {
                std::vector<CoordinateType> coords;
                std::vector<float> values;

                // Get the range of indices for the coordinate range
                int64_t start_idx = timeframe_ptr->getIndexAtTime(start_coord);
                int64_t end_idx = timeframe_ptr->getIndexAtTime(end_coord);

                if (start_idx > end_idx) std::swap(start_idx, end_idx);

                coords.reserve(static_cast<size_t>(end_idx - start_idx + 1));
                values.reserve(static_cast<size_t>(end_idx - start_idx + 1));

                for (int64_t idx = start_idx; idx <= end_idx; ++idx) {
                    if (idx >= 0 && static_cast<size_t>(idx) < _data.size()) {
                        coords.push_back(timeframe_ptr->getTimeAtIndex(idx));
                        values.push_back(_data[static_cast<size_t>(idx)]);
                    }
                }

                return {coords, values};
            } else {
                throw std::runtime_error("Coordinate type mismatch with TimeFrameV2");
            }
        },
                          _timeframe_v2.value());
    }

protected:
private:
    std::vector<float> _data;
    std::vector<size_t> _time;

    // New TimeFrameV2 support
    std::optional<AnyTimeFrame> _timeframe_v2;
};

/**
 * @brief Calculate the mean value of an AnalogTimeSeries
 *
 * @param series The time series to calculate the mean from
 * @return float The mean value
 */
float calculate_mean(AnalogTimeSeries const & series);

/**
 * @brief Calculate the mean value of an AnalogTimeSeries in a specific range
 *
 * @param series The time series to calculate the mean from
 * @param start Start index of the range (inclusive)
 * @param end End index of the range (exclusive)
 * @return float The mean value in the specified range
 */
float calculate_mean(AnalogTimeSeries const & series, int64_t start, int64_t end);

/**
 * @brief Calculate the standard deviation of an AnalogTimeSeries
 *
 * @param series The time series to calculate the standard deviation from
 * @return float The standard deviation
 */
float calculate_std_dev(AnalogTimeSeries const & series);

/**
 * @brief Calculate the standard deviation of an AnalogTimeSeries in a specific range
 *
 * @param series The time series to calculate the standard deviation from
 * @param start Start index of the range (inclusive)
 * @param end End index of the range (exclusive)
 * @return float The standard deviation in the specified range
 */
float calculate_std_dev(AnalogTimeSeries const & series, int64_t start, int64_t end);

/**
 * @brief Calculate the minimum value in an AnalogTimeSeries
 *
 * @param series The time series to find the minimum value in
 * @return float The minimum value
 */
float calculate_min(AnalogTimeSeries const & series);

/**
 * @brief Calculate the minimum value in an AnalogTimeSeries in a specific range
 *
 * @param series The time series to find the minimum value in
 * @param start Start index of the range (inclusive)
 * @param end End index of the range (exclusive)
 * @return float The minimum value in the specified range
 */
float calculate_min(AnalogTimeSeries const & series, int64_t start, int64_t end);

/**
 * @brief Calculate the maximum value in an AnalogTimeSeries
 *
 * @param series The time series to find the maximum value in
 * @return float The maximum value
 */
float calculate_max(AnalogTimeSeries const & series);

/**
 * @brief Calculate the maximum value in an AnalogTimeSeries in a specific range
 *
 * @param series The time series to find the maximum value in
 * @param start Start index of the range (inclusive)
 * @param end End index of the range (exclusive)
 * @return float The maximum value in the specified range
 */
float calculate_max(AnalogTimeSeries const & series, int64_t start, int64_t end);

/**
 * @brief Calculate an approximate standard deviation using systematic sampling
 *
 * Uses systematic sampling (every Nth element) to estimate standard deviation efficiently.
 * If the sample size would be below the minimum threshold, falls back to exact calculation.
 *
 * @param series The time series to calculate the standard deviation from
 * @param sample_percentage Percentage of data to sample (e.g., 0.1 for 0.1%)
 * @param min_sample_threshold Minimum number of samples before falling back to exact calculation
 * @return float The approximate standard deviation
 */
float calculate_std_dev_approximate(AnalogTimeSeries const & series,
                                    float sample_percentage = 0.1f,
                                    size_t min_sample_threshold = 1000);

/**
 * @brief Calculate an approximate standard deviation using adaptive sampling
 *
 * Starts with a small sample and progressively increases until the estimate
 * converges within the specified tolerance or reaches maximum samples.
 *
 * @param series The time series to calculate the standard deviation from
 * @param initial_sample_size Starting number of samples
 * @param max_sample_size Maximum number of samples to use
 * @param convergence_tolerance Relative tolerance for convergence (e.g., 0.01 for 1%)
 * @return float The approximate standard deviation
 */
float calculate_std_dev_adaptive(AnalogTimeSeries const & series,
                                 size_t initial_sample_size = 100,
                                 size_t max_sample_size = 10000,
                                 float convergence_tolerance = 0.01f);

#endif// ANALOG_TIME_SERIES_HPP
