#ifndef ANALOG_TIME_SERIES_HPP
#define ANALOG_TIME_SERIES_HPP

#include "Observer/Observer_Data.hpp"

#include <algorithm>
#include <cmath> // std::nan
#include <map>
#include <numeric>
#include <string>
#include <vector>

/**
 * @brief The AnalogTimeSeries class
 *
 *
 */
class AnalogTimeSeries : public ObserverData {
public:
    AnalogTimeSeries() = default;
    AnalogTimeSeries(std::vector<float> analog_vector);
    AnalogTimeSeries(std::vector<float> analog_vector, std::vector<size_t> time_vector);
    AnalogTimeSeries(std::map<int, float> analog_map);

    void setData(std::vector<float> analog_vector) {
        _data.clear();
        _data = analog_vector;
        _time = std::vector<size_t>(analog_vector.size());
        std::iota(_time.begin(), _time.end(), 0);
    };
    void setData(std::vector<float> analog_vector, std::vector<size_t> time_vector) {
        _data.clear();
        _time.clear();
        _data = analog_vector;
        _time = time_vector;
    };
    void setData(std::map<int, float> analog_map) {
        _data.clear();
        _time.clear();
        _data = std::vector<float>();
        _time = std::vector<size_t>();
        for (auto& [key, value] : analog_map) {
            _time.push_back(key);
            _data.push_back(value);
        }
    };

    std::vector<float>& getAnalogTimeSeries() {return _data;};
    std::vector<size_t>& getTimeSeries() {return _time;};

    float getMinValue() const {
        return *std::min_element(_data.begin(), _data.end());
    }

    float getMinValue(size_t start, size_t end) const {
        return *std::min_element(_data.begin() + start, _data.begin() + end);
    }

    float getMaxValue() const {
        return *std::max_element(_data.begin(), _data.end());
    }

    float getMaxValue(size_t start, size_t end) const {
        return *std::max_element(_data.begin() + start, _data.begin() + end);
    }

protected:

private:
    std::vector<float> _data;
    std::vector<size_t> _time;
};

/**
 * @brief load_analog_series_from_csv
 *
 *
 *
 * @param filename - the name of the file to load
 * @return a vector of floats representing the analog time series
 */
std::vector<float> load_analog_series_from_csv(std::string const& filename);

#endif // ANALOG_TIME_SERIES_HPP
