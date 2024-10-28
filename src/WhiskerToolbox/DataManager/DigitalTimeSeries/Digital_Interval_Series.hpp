#ifndef DIGITAL_INTERVAL_SERIES_HPP
#define DIGITAL_INTERVAL_SERIES_HPP

#include <string>
#include <vector>
#include <utility>

class DigitalIntervalSeries {
public:
    DigitalIntervalSeries() = default;
    DigitalIntervalSeries(std::vector<std::pair<float, float>> digital_vector);

    void setData(std::vector<std::pair<float, float>> digital_vector) { _data = digital_vector;};
    std::vector<std::pair<float, float>> const& getDigitalIntervalSeries() const;

private:
    std::vector<std::pair<float, float>> _data {};

};

std::vector<std::pair<float, float>> load_digital_series_from_csv(std::string const& filename);

#endif // DIGITAL_INTERVAL_SERIES_HPP