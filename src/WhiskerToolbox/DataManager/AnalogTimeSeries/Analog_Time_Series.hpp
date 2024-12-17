#ifndef ANALOG_TIME_SERIES_HPP
#define ANALOG_TIME_SERIES_HPP

#include <algorithm>
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

    void setData(std::vector<float> analog_vector) { _data = analog_vector;};

    std::vector<float> const& getAnalogTimeSeries() const;

    float getMinValue() const {
        return *std::min_element(_data.begin(), _data.end());
    }

    float getMaxValue() const {
        return *std::max_element(_data.begin(), _data.end());
    }
protected:

private:
    std::vector<float> _data {};
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
