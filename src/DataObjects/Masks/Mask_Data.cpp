#include "Mask_Data.hpp"

#include "Entity/EntityRegistry.hpp"
#include "Masks/utils/mask_utils.hpp"
#include "RaggedTimeSeries/map_timeseries.hpp"

#include <iostream>

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

    ImageSize const old_size = _image_size;

    for (size_t i = 0; i < _storage.size(); ++i) {
        Mask2D & mask = _storage.getMutableData(i);
        if (mask.empty()) {
            continue;
        }
        mask = resize_mask(mask, old_size, image_size);
    }
    _image_size = image_size;
}
