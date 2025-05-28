#include "Mask_Data.hpp"

#include <algorithm>
#include <iostream>


bool MaskData::clearAtTime(size_t const time, bool notify) {
    auto it = std::find(_time.begin(), _time.end(), time);
    if (it != _time.end()) {
        size_t const index = std::distance(_time.begin(), it);
        _data[index].clear();

        if (notify) {
            notifyObservers();
        }
        return true;
    }

    // No masks exist at this time, nothing to clear
    return false;
}

void MaskData::addAtTime(size_t const time,
                             std::vector<float> const & x,
                             std::vector<float> const & y,
                             bool notify) {
    auto new_mask = create_mask(x, y);

    auto it = std::find(_time.begin(), _time.end(), time);
    if (it != _time.end()) {
        size_t const index = std::distance(_time.begin(), it);
        _data[index].push_back(new_mask);
    } else {
        _time.push_back(time);
        _data.push_back({new_mask});
    }
    if (notify) {
        notifyObservers();
    }
}

void MaskData::addAtTime(size_t const time,
                             std::vector<Point2D<float>> mask,
                             bool notify) {
    auto it = std::find(_time.begin(), _time.end(), time);
    if (it != _time.end()) {
        size_t const index = std::distance(_time.begin(), it);
        _data[index].push_back(std::move(mask));
    } else {
        _time.push_back(time);
        _data.push_back({std::move(mask)});
    }
    if (notify) {
        notifyObservers();
    }
}

void MaskData::addAtTime(size_t const time,
                             std::vector<float> && x,
                             std::vector<float> && y,
                             bool notify) {
    // Create mask efficiently using move semantics
    auto new_mask = Mask2D{};
    new_mask.reserve(x.size());
    
    for (std::size_t i = 0; i < x.size(); i++) {
        new_mask.emplace_back(x[i], y[i]);
    }

    auto it = std::find(_time.begin(), _time.end(), time);
    if (it != _time.end()) {
        size_t const index = std::distance(_time.begin(), it);
        _data[index].push_back(std::move(new_mask));
    } else {
        _time.push_back(time);
        _data.push_back({std::move(new_mask)});
    }
    if (notify) {
        notifyObservers();
    }
}

std::vector<Mask2D> const & MaskData::getAtTime(size_t const time) const {
    auto it = std::find(_time.begin(), _time.end(), time);
    if (it != _time.end()) {
        size_t const index = std::distance(_time.begin(), it);
        return _data[index];
    } else {
        return _empty;
    }
}

std::vector<size_t> MaskData::getTimesWithData() {
    std::vector<size_t> times;
    times.reserve(_time.size());
    for (auto t: _time) {
        times.push_back(t);
    }
    return times;
}

void MaskData::changeImageSize(ImageSize const & image_size)
{
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

    for (auto & masks : _data) {
        for (auto & mask : masks) {
            for (auto & point : mask) {
                point.x *= scale_x;
                point.y *= scale_y;
            }
        }
    }
    _image_size = image_size;

}

void MaskData::reserveCapacity(size_t capacity) {
    _time.reserve(capacity);
    _data.reserve(capacity);
}
