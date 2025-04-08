
#include "data_transforms.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "Masks/Mask_Data.hpp"
#include "Media/Media_Data.hpp"
#include "Points/Point_Data.hpp"

#include <map>
#include <numeric>


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

void scale(std::shared_ptr<PointData> & point_data, ImageSize const & image_size_media) {
    auto image_size_point = point_data->getImageSize();

    auto const media_height = image_size_media.height;
    auto const media_width = image_size_media.width;
    auto const point_height = image_size_point.height;
    auto const point_width = image_size_point.width;

    if (media_width == point_width && media_height == media_width) {
        return;
    }

    if (point_width == -1 || point_height == -1) {
        return;
    }

    float const height_ratio = static_cast<float>(media_height) / static_cast<float>(point_height);
    float const width_ratio = static_cast<float>(media_width) / static_cast<float>(point_width);


    for (auto & [timestamp, points]: point_data->getData()) {
        auto scaled_points = std::vector<Point2D<float>>();
        for (auto & point: points) {
            scaled_points.push_back(
                    {point.x * height_ratio,
                     point.y * width_ratio});
        }
        point_data->overwritePointsAtTime(timestamp, scaled_points);
    }

    point_data->setImageSize(ImageSize());
}
