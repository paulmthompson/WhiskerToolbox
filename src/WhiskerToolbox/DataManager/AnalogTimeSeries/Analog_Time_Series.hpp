#ifndef ANALOG_TIME_SERIES_HPP
#define ANALOG_TIME_SERIES_HPP

#include "Observer/Observer_Data.hpp"


#include <concepts>
#include <cstdint>
#include <map>
#include <numeric>
#include <ranges>
#include <string>
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

    [[nodiscard]] std::vector<float> getAnalogTimeSeries() const { return _data; };
    [[nodiscard]] std::vector<size_t> getTimeSeries() const { return _time; };

    template<typename TransformFunc = std::identity>
    auto getDataInRange(float start_time, float stop_time,
                        TransformFunc && time_transform = {}) const {
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

    template<typename TransformFunc = std::identity>
    std::pair<std::vector<size_t>, std::vector<float>> getDataVectorsInRange(
            float start_time, float stop_time,
            TransformFunc && time_transform = {}) const {
        std::vector<size_t> times;
        std::vector<float> values;

        auto range = getDataInRange(start_time, stop_time, time_transform);
        for (auto const & point: range) {
            times.push_back(point.time_idx);
            values.push_back(point.value);
        }

        return {times, values};
    }

protected:
private:
    std::vector<float> _data;
    std::vector<size_t> _time;
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

#endif// ANALOG_TIME_SERIES_HPP
