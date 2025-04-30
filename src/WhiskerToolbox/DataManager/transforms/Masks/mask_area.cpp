
#include "mask_area.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "Masks/Mask_Data.hpp"

#include <map>

std::shared_ptr<AnalogTimeSeries> area(std::shared_ptr<MaskData> const & mask_data) {
    auto analog_time_series = std::make_shared<AnalogTimeSeries>();
    std::map<int, float> areas;

    for (auto const & [timestamp, masks]: mask_data->getData()) {
        float area = 0.0f;
        for (auto const & mask: masks) {
            area += static_cast<float>(mask.size());
        }
        areas[timestamp] = area;
    }

    analog_time_series->setData(areas);

    return analog_time_series;
}