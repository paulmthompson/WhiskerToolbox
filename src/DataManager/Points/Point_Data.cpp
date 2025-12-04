#include "Point_Data.hpp"

#include "Entity/EntityRegistry.hpp"
#include "utils/map_timeseries.hpp"

#include <algorithm>
#include <iostream>
#include <ranges>

// ========== Constructors ==========

PointData::PointData(std::map<TimeFrameIndex, Point2D<float>> const & data) {
    for (auto const & [time, point]: data) {
        _storage.append(time, point, EntityId(0));
    }
}

PointData::PointData(std::map<TimeFrameIndex, std::vector<Point2D<float>>> const & data) {
    for (auto const & [time, points]: data) {
        for (auto const & p: points) {
            _storage.append(time, p, EntityId(0));
        }
    }
}

// ========== Image Size ==========

void PointData::changeImageSize(ImageSize const & image_size) {
    if (_image_size.width == -1 || _image_size.height == -1) {
        std::cout << "No size set for current image. "
                  << " Please set a valid image size before trying to scale" << std::endl;
        _image_size = image_size;// Set the image size if it wasn't set before
        return;
    }

    if (_image_size.width == image_size.width && _image_size.height == image_size.height) {
        std::cout << "Image size is the same. No need to scale" << std::endl;
        return;
    }

    float const scale_x = static_cast<float>(image_size.width) / static_cast<float>(_image_size.width);
    float const scale_y = static_cast<float>(image_size.height) / static_cast<float>(_image_size.height);

    for (size_t i = 0; i < _storage.size(); ++i) {
        Point2D<float>& point = _storage.getMutableData(i);
        point.x *= scale_x;
        point.y *= scale_y;
    }
    _image_size = image_size;
}
