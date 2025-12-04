#include "Mask_Data.hpp"

#include "Entity/EntityRegistry.hpp"
#include "utils/map_timeseries.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <ranges>

// ========== Constructors ==========


// ========== Image Size ==========

void MaskData::changeImageSize(ImageSize const & image_size) {
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
        Mask2D& mask = _storage.getMutableData(i);
        for (auto & point: mask) {
            point.x = static_cast<uint32_t>(std::round(static_cast<float>(point.x) * scale_x));
            point.y = static_cast<uint32_t>(std::round(static_cast<float>(point.y) * scale_y));
        }
    }
    _image_size = image_size;
}
