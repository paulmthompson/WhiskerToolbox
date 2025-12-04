#include "Line_Data.hpp"

#include "CoreGeometry/points.hpp"
#include "Entity/EntityRegistry.hpp"
#include "utils/map_timeseries.hpp"

#include <cmath>
#include <iostream>
#include <ranges>

// ========== Constructors ==========

LineData::LineData(std::map<TimeFrameIndex, std::vector<Line2D>> const & data) {
    // Convert old format to new SoA storage
    for (auto const & [time, lines]: data) {
        for (auto const & line: lines) {
            _storage.append(time, line, EntityId(0));// EntityId will be 0 initially
        }
    }
}

// ========== Image Size ==========

void LineData::changeImageSize(ImageSize const & image_size) {
    if (_image_size.width == -1 || _image_size.height == -1) {
        std::cout << "No size set for current image. "
                  << " Please set a valid image size before trying to scale" << std::endl;
    }

    if (_image_size.width == image_size.width && _image_size.height == image_size.height) {
        std::cout << "Image size is the same. No need to scale" << std::endl;
        return;
    }

    float const scale_x = static_cast<float>(image_size.width) / static_cast<float>(_image_size.width);
    float const scale_y = static_cast<float>(image_size.height) / static_cast<float>(_image_size.height);

    for (size_t i = 0; i < _storage.size(); ++i) {
        Line2D& line = _storage.getMutableData(i);
        for (auto & point: line) {
            point.x *= scale_x;
            point.y *= scale_y;
        }
    }
    _image_size = image_size;
}
