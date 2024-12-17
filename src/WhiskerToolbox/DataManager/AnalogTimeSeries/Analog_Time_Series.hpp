#ifndef ANALOG_TIME_SERIES_HPP
#define ANALOG_TIME_SERIES_HPP

#include <algorithm>
#include <cmath> // std::nan
#include <map>
#include <string>
#include <vector>

/**
 * @brief The AnalogTimeSeries class
 *
 *
 */
class AnalogTimeSeries {
public:
    AnalogTimeSeries() = default;
    AnalogTimeSeries(std::vector<float> analog_vector);
    AnalogTimeSeries(std::map<int, float> analog_map);

    void setData(std::map<int, float> analog_map) { _data = analog_map; };
    void setData(std::vector<float> analog_vector) {
        _data.clear();
        for (int i = 0; i < analog_vector.size(); i++) {
            _data[i] = analog_vector[i];
        }
    };

    std::map<int, float> const& getAnalogTimeSeries() const;

    float getMinValue() const {
        auto min_elem = std::min_element(_data.begin(), _data.end(),
                                         [](const auto& lhs, const auto& rhs) {
                                             return lhs.second < rhs.second;
                                         });
        return min_elem->second;
    }

    float getMaxValue() const {
        auto max_elem = std::max_element(_data.begin(), _data.end(),
                                         [](const auto& lhs, const auto& rhs) {
                                             return lhs.second < rhs.second;
                                         });
        return max_elem->second;
    }

    std::vector<float> getVector() const {
        if (_data.empty()) {
            return {};
        }

        int minKey = _data.begin()->first;
        int maxKey = _data.rbegin()->first;
        std::vector<float> result(maxKey - minKey + 1, std::nanf(""));

        for (const auto& [key, value] : _data) {
            result[key - minKey] = value;
        }

        return result;
    }
protected:

private:
    //std::vector<float> _data {};
    std::map<int, float> _data {};
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
