#include "Mask_Data.hpp"

#include <algorithm>
#include <iostream>

bool MaskData::clearAtTime(int const time, bool notify) {
    auto it = _data.find(time);
    if (it != _data.end()) {
        _data.erase(it);
        if (notify) {
            notifyObservers();
        }
        return true;
    }

    // No masks exist at this time, nothing to clear
    return false;
}

void MaskData::addAtTime(int const time,
                             std::vector<float> const & x,
                             std::vector<float> const & y,
                             bool notify) {
    auto new_mask = create_mask(x, y);
    _data[time].push_back(new_mask);
    
    if (notify) {
        notifyObservers();
    }
}

void MaskData::addAtTime(int const time,
                             std::vector<Point2D<float>> mask,
                             bool notify) {
    _data[time].push_back(std::move(mask));
    
    if (notify) {
        notifyObservers();
    }
}

void MaskData::addAtTime(int const time,
                             std::vector<float> && x,
                             std::vector<float> && y,
                             bool notify) {
    // Create mask efficiently using move semantics
    auto new_mask = Mask2D{};
    new_mask.reserve(x.size());
    
    for (std::size_t i = 0; i < x.size(); i++) {
        //new_mask.emplace_back(x[i], y[i]);
        new_mask.push_back({x[i], y[i]});
    }

    _data[time].push_back(std::move(new_mask));
    
    if (notify) {
        notifyObservers();
    }
}

std::vector<Mask2D> const & MaskData::getAtTime(int const time) const {
    auto it = _data.find(time);
    if (it != _data.end()) {
        return it->second;
    } else {
        return _empty;
    }
}

std::vector<int> MaskData::getTimesWithData() const {
    std::vector<int> times;
    times.reserve(_data.size());
    for (auto const & [time, masks] : _data) {
        times.push_back(time);
    }
    return times;
}

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

    for (auto & [time, masks] : _data) {
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

    static_cast<void>(capacity);
    // Note: std::map doesn't have a reserve function, but this is kept for API compatibility
    // The map will allocate nodes as needed
}

std::size_t MaskData::copyTo(MaskData& target, int start_time, int end_time, bool notify) const {
    if (start_time > end_time) {
        std::cerr << "MaskData::copyTo: start_time (" << start_time 
                  << ") must be <= end_time (" << end_time << ")" << std::endl;
        return 0;
    }

    // Ensure target is not the same as source
    if (this == &target) {
        std::cerr << "MaskData::copyTo: Cannot copy to self" << std::endl;
        return 0;
    }

    std::size_t total_masks_copied = 0;

    // Iterate through all times in the source data within the range
    for (auto const & [time, masks] : _data) {
        if (time >= start_time && time <= end_time && !masks.empty()) {
            for (auto const& mask : masks) {
                target.addAtTime(time, mask, false); // Don't notify for each operation
                total_masks_copied++;
            }
        }
    }

    // Notify observer only once at the end if requested
    if (notify && total_masks_copied > 0) {
        target.notifyObservers();
    }

    return total_masks_copied;
}

std::size_t MaskData::copyTo(MaskData& target, std::vector<int> const& times, bool notify) const {
    std::size_t total_masks_copied = 0;

    // Copy masks for each specified time
    for (int time : times) {
        auto it = _data.find(time);
        if (it != _data.end() && !it->second.empty()) {
            for (auto const& mask : it->second) {
                target.addAtTime(time, mask, false); // Don't notify for each operation
                total_masks_copied++;
            }
        }
    }

    // Notify observer only once at the end if requested
    if (notify && total_masks_copied > 0) {
        target.notifyObservers();
    }

    return total_masks_copied;
}

std::size_t MaskData::moveTo(MaskData& target, int start_time, int end_time, bool notify) {
    if (start_time > end_time) {
        std::cerr << "MaskData::moveTo: start_time (" << start_time 
                  << ") must be <= end_time (" << end_time << ")" << std::endl;
        return 0;
    }

    std::size_t total_masks_moved = 0;
    std::vector<int> times_to_clear;

    // First, copy all masks in the range to target
    for (auto const & [time, masks] : _data) {
        if (time >= start_time && time <= end_time && !masks.empty()) {
            for (auto const& mask : masks) {
                target.addAtTime(time, mask, false); // Don't notify for each operation
                total_masks_moved++;
            }
            times_to_clear.push_back(time);
        }
    }

    // Then, clear all the times from source
    for (int time : times_to_clear) {
        clearAtTime(time, false); // Don't notify for each operation
    }

    // Notify observers only once at the end if requested
    if (notify && total_masks_moved > 0) {
        target.notifyObservers();
        notifyObservers();
    }

    return total_masks_moved;
}

std::size_t MaskData::moveTo(MaskData& target, std::vector<int> const& times, bool notify) {
    std::size_t total_masks_moved = 0;
    std::vector<int> times_to_clear;

    // First, copy masks for each specified time to target
    for (int time : times) {
        auto it = _data.find(time);
        if (it != _data.end() && !it->second.empty()) {
            for (auto const& mask : it->second) {
                target.addAtTime(time, mask, false); // Don't notify for each operation
                total_masks_moved++;
            }
            times_to_clear.push_back(time);
        }
    }

    // Then, clear all the times from source
    for (int time : times_to_clear) {
        clearAtTime(time, false); // Don't notify for each operation
    }

    // Notify observers only once at the end if requested
    if (notify && total_masks_moved > 0) {
        target.notifyObservers();
        notifyObservers();
    }

    return total_masks_moved;
}
