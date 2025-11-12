#include "Mask_Data.hpp"

#include "Entity/EntityRegistry.hpp"
#include "utils/map_timeseries.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <ranges>

// ========== Constructors ==========

// ========== Setters ==========

void MaskData::addAtTime(TimeFrameIndex const time, Mask2D const & mask, NotifyObservers notify) {
    int const local_index = static_cast<int>(_data[time].size());
    auto entity_id = EntityId(0);
    if (_identity_registry) {
        entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::MaskEntity, time, local_index);
    }

    _data[time].emplace_back(entity_id, mask);

    if (notify == NotifyObservers::Yes) {
        notifyObservers();
    }
}

void MaskData::addAtTime(TimeFrameIndex const time, Mask2D && mask, NotifyObservers notify) {
    int const local_index = static_cast<int>(_data[time].size());
    auto entity_id = EntityId(0);
    if (_identity_registry) {
        entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::MaskEntity, time, local_index);
    }

    _data[time].emplace_back(entity_id, std::move(mask));

    if (notify == NotifyObservers::Yes) {
        notifyObservers();
    }
}

void MaskData::addAtTime(TimeFrameIndex const time,
                         std::vector<uint32_t> const & x,
                         std::vector<uint32_t> const & y,
                         NotifyObservers notify) {
    auto new_mask = create_mask(x, y);
    int const local_index = static_cast<int>(_data[time].size());
    auto entity_id = EntityId(0);
    if (_identity_registry) {
        entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::MaskEntity, time, local_index);
    }
    _data[time].emplace_back(entity_id, std::move(new_mask));

    if (notify == NotifyObservers::Yes) {
        notifyObservers();
    }
}


void MaskData::addAtTime(TimeIndexAndFrame const & time_index_and_frame,
                         std::vector<Point2D<uint32_t>> mask,
                         NotifyObservers notify) {
    if (time_index_and_frame.time_frame == _time_frame.get()) {
        addAtTime(time_index_and_frame.index, std::move(mask), notify);
        return;
    }

    if (!time_index_and_frame.time_frame || !_time_frame.get()) {
        return;
    }

    auto time = time_index_and_frame.time_frame->getTimeAtIndex(time_index_and_frame.index);
    auto time_index = _time_frame->getIndexAtTime(static_cast<float>(time));

    int const local_index = static_cast<int>(_data[time_index].size());
    auto entity_id = EntityId(0);
    if (_identity_registry) {
        entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::MaskEntity, time_index, local_index);
    }
    _data[time_index].emplace_back(entity_id, std::move(mask));
}

void MaskData::addAtTime(TimeFrameIndex const time,
                         std::vector<uint32_t> && x,
                         std::vector<uint32_t> && y,
                         NotifyObservers notify) {
    // Create mask efficiently using move semantics
    auto new_mask = Mask2D{};
    auto xx = std::move(x);
    auto yy = std::move(y);
    new_mask.reserve(xx.size());

    for (std::size_t i = 0; i < xx.size(); i++) {
        new_mask.emplace_back(xx[i], yy[i]);
    }

    int const local_index = static_cast<int>(_data[time].size());
    auto entity_id = EntityId(0);
    if (_identity_registry) {
        entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::MaskEntity, time, local_index);
    }
    _data[time].emplace_back(entity_id, std::move(new_mask));

    if (notify == NotifyObservers::Yes) {
        notifyObservers();
    }
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

    for (auto & [time, entries]: _data) {
        (void) time;
        for (auto & entry: entries) {
            for (auto & point: entry.data) {
                point.x = static_cast<uint32_t>(std::round(static_cast<float>(point.x) * scale_x));
                point.y = static_cast<uint32_t>(std::round(static_cast<float>(point.y) * scale_y));
            }
        }
    }
    _image_size = image_size;
}
