#include "Mask_Data.hpp"

#include "Entity/EntityRegistry.hpp"
#include "utils/map_timeseries.hpp"
#include "utils/mask_utils.hpp"

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
        return;
    }

    if (_image_size.width == image_size.width && _image_size.height == image_size.height) {
        std::cout << "Image size is the same. No need to scale" << std::endl;
        return;
    }

    ImageSize const source_size = _image_size;

    for (size_t i = 0; i < _storage.size(); ++i) {
        Mask2D& mask = _storage.getMutableData(i);
        mask = resize_mask(mask, source_size, image_size);
    }
    _image_size = image_size;
}
