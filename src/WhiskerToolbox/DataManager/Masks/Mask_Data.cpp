#include "Mask_Data.hpp"

#include <algorithm>
#include <iostream>
#include <string>


void MaskData::clearMasksAtTime(size_t const time) {
    auto it = std::find(_time.begin(), _time.end(), time);
    if (it != _time.end()) {
        size_t const index = std::distance(_time.begin(), it);
        _data[index].clear();
    } else {
        // If time doesn't exist, add it with empty vector
        _time.push_back(time);
        _data.emplace_back();
    }
    notifyObservers();
}

void MaskData::addMaskAtTime(size_t const time, std::vector<float> const & x, std::vector<float> const & y) {
    auto new_mask = create_mask(x, y);

    auto it = std::find(_time.begin(), _time.end(), time);
    if (it != _time.end()) {
        size_t const index = std::distance(_time.begin(), it);
        _data[index].push_back(new_mask);
    } else {
        _time.push_back(time);
        _data.push_back({new_mask});
    }
    notifyObservers();
}

void MaskData::addMaskAtTime(size_t const time, std::vector<Point2D<float>> mask) {
    auto it = std::find(_time.begin(), _time.end(), time);
    if (it != _time.end()) {
        size_t const index = std::distance(_time.begin(), it);
        _data[index].push_back(std::move(mask));
    } else {
        _time.push_back(time);
        _data.push_back({std::move(mask)});
    }
    notifyObservers();
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
