
#include "data_transforms.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "Media/Media_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"

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

void scale(std::shared_ptr<PointData>& point_data, ImageSize const& image_size_media)
{
    auto image_size_point = point_data->getImageSize();

    const auto media_height = image_size_media.getHeight();
    const auto media_width = image_size_media.getWidth();
    const auto point_height = image_size_point.getHeight();
    const auto point_width = image_size_point.getWidth();

    if (media_width == point_width && media_height == media_width) {
        return;
    }

    if (point_width == -1 || point_height == -1) {
        return;
    }

    for (auto& [timestamp, points] : point_data->getData()) {
        auto scaled_points = std::vector<Point2D<float>>();
        for (auto& point : points) {
            scaled_points.push_back(
                {point.x * media_height / point_height,
                point.y * media_width / point_width});
        }
        point_data->overwritePointsAtTime(timestamp,scaled_points);
    }

    point_data->setImageSize({-1, -1});
}
