#include "Mask_Data.hpp"

#include <algorithm>
#include <iostream>
#include <string>


void MaskData::clearMasksAtTime(size_t const time, bool notify) {
    auto it = std::find(_time.begin(), _time.end(), time);
    if (it != _time.end()) {
        size_t const index = std::distance(_time.begin(), it);
        _data[index].clear();

        if (notify) {
            notifyObservers();
        }
    }
    // If time doesn't exist, do nothing (don't add an empty vector)
}

void MaskData::addMaskAtTime(size_t const time,
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

void MaskData::addMaskAtTime(size_t const time,
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

std::vector<Mask2D> const & MaskData::getMasksAtTime(size_t const time) const {
    auto it = std::find(_time.begin(), _time.end(), time);
    if (it != _time.end()) {
        size_t const index = std::distance(_time.begin(), it);
        return _data[index];
    } else {
        return _empty;
    }
}
