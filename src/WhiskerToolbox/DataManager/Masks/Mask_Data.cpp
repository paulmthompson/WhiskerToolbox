#include "Mask_Data.hpp"

#include "utils/map_timeseries.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

// ========== Constructors ==========

// ========== Setters ==========

bool MaskData::clearAtTime(TimeFrameIndex const time, bool notify) {
    if (clear_at_time(time, _data)) {
        if (notify) {
            notifyObservers();
        }
        return true;
    }
    return false;
}

bool MaskData::clearAtTime(TimeFrameIndex const time, size_t const index, bool notify) {
    if (clear_at_time(time, index, _data)) {
        if (notify) {
            notifyObservers();
        }
        return true;
    }
    return false;
}

void MaskData::addAtTime(TimeFrameIndex const time,
                             std::vector<uint32_t> const & x,
                             std::vector<uint32_t> const & y,
                             bool notify) {
    auto new_mask = create_mask(x, y);
    add_at_time(time, std::move(new_mask), _data);
    
    if (notify) {
        notifyObservers();
    }
}

void MaskData::addAtTime(TimeFrameIndex const time,
                             std::vector<Point2D<uint32_t>> mask,
                             bool notify) {
    add_at_time(time, std::move(mask), _data);
    
    if (notify) {
        notifyObservers();
    }
}
        
void MaskData::addAtTime(TimeFrameIndex const time,
                             std::vector<uint32_t> && x,
                             std::vector<uint32_t> && y,
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

// ========== Getters ==========

std::vector<Mask2D> const & MaskData::getAtTime(TimeFrameIndex const time) const {
    return get_at_time(time, _data, _empty);
}

std::vector<Mask2D> const & MaskData::getAtTime(TimeFrameIndex const time, 
                                                TimeFrame const * source_timeframe,
                                                TimeFrame const * mask_timeframe) const {

    return get_at_time(time, _data, _empty, source_timeframe, mask_timeframe);
}

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

    for (auto & [time, masks] : _data) {
        for (auto & mask : masks) {
            for (auto & point : mask) {
                point.x = static_cast<uint32_t>(std::round(static_cast<float>(point.x) * scale_x));
                point.y = static_cast<uint32_t>(std::round(static_cast<float>(point.y) * scale_y));
            }
        }
    }
    _image_size = image_size;
}

// ========== Copy and Move ==========

std::size_t MaskData::copyTo(MaskData& target, TimeFrameInterval const & interval, bool notify) const {
    if (interval.start > interval.end) {
        std::cerr << "MaskData::copyTo: interval start (" << interval.start.getValue() 
                  << ") must be <= interval end (" << interval.end.getValue() << ")" << std::endl;
        return 0;
    }

    // Ensure target is not the same as source
    if (this == &target) {
        std::cerr << "MaskData::copyTo: Cannot copy to self" << std::endl;
        return 0;
    }

    std::size_t total_masks_copied = 0;

    // Iterate through all times in the source data within the interval
    for (auto const & [time, masks] : _data) {
        if (time >= interval.start && time <= interval.end && !masks.empty()) {
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

std::size_t MaskData::copyTo(MaskData& target, std::vector<TimeFrameIndex> const& times, bool notify) const {
    std::size_t total_masks_copied = 0;

    // Copy masks for each specified time
    for (TimeFrameIndex time : times) {
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

std::size_t MaskData::moveTo(MaskData& target, TimeFrameInterval const & interval, bool notify) {
    if (interval.start > interval.end) {
        std::cerr << "MaskData::moveTo: interval start (" << interval.start.getValue() 
                  << ") must be <= interval end (" << interval.end.getValue() << ")" << std::endl;
        return 0;
    }

    std::size_t total_masks_moved = 0;
    std::vector<TimeFrameIndex> times_to_clear;

    // First, copy all masks in the interval to target
    for (auto const & [time, masks] : _data) {
        if (time >= interval.start && time <= interval.end && !masks.empty()) {
            for (auto const& mask : masks) {
                target.addAtTime(time, mask, false); // Don't notify for each operation
                total_masks_moved++;
            }
            times_to_clear.push_back(time);
        }
    }

    // Then, clear all the times from source
    for (TimeFrameIndex time : times_to_clear) {
        clearAtTime(time, false); // Don't notify for each operation
    }

    // Notify observers only once at the end if requested
    if (notify && total_masks_moved > 0) {
        target.notifyObservers();
        notifyObservers();
    }

    return total_masks_moved;
}

std::size_t MaskData::moveTo(MaskData& target, std::vector<TimeFrameIndex> const& times, bool notify) {
    std::size_t total_masks_moved = 0;
    std::vector<TimeFrameIndex> times_to_clear;

    // First, copy masks for each specified time to target
    for (TimeFrameIndex time : times) {
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
    for (TimeFrameIndex time : times_to_clear) {
        clearAtTime(time, false); // Don't notify for each operation
    }

    // Notify observers only once at the end if requested
    if (notify && total_masks_moved > 0) {
        target.notifyObservers();
        notifyObservers();
    }

    return total_masks_moved;
}
