
#include "data_transforms.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "Masks/Mask_Data.hpp"

#include <map>
#include <numeric>


std::shared_ptr<AnalogTimeSeries> area(const std::shared_ptr<MaskData>& mask_data)
{
    auto analog_time_series = std::make_shared<AnalogTimeSeries>();
    std::map<int, float> areas;

    for (const auto& [timestamp, masks] : mask_data->getData()) {
        float area = 0.0f;
        for (const auto& mask : masks) {
            area += mask.size();
        }
        areas[timestamp] = area;
    }

    analog_time_series->setData(areas);

    return analog_time_series;
}
