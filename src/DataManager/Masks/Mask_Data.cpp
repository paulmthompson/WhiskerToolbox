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

// Implement copyByEntityIds and moveByEntityIds for MaskData
std::size_t MaskData::copyByEntityIds(MaskData & target,
                                      std::unordered_set<EntityId> const & entity_ids,
                                      NotifyObservers notify) {
    std::size_t count = 0;

    ImageSize const src_size = this->getImageSize();
    ImageSize const dst_size = target.getImageSize();

    bool need_scale = (src_size.width > 0 && src_size.height > 0 &&
                       dst_size.width > 0 && dst_size.height > 0 &&
                       (src_size.width != dst_size.width || src_size.height != dst_size.height));

    for (size_t i = 0; i < _storage.size(); ++i) {
        EntityId const eid = _storage.getEntityId(i);
        if (!entity_ids.contains(eid)) continue;

        TimeFrameIndex const time = _storage.getTime(i);
        Mask2D const & src_mask = _storage.getData(i);

        if (!need_scale) {
            target.addAtTime(time, src_mask, NotifyObservers::No);
        } else {
            int const src_w = std::max(1, src_size.width);
            int const src_h = std::max(1, src_size.height);
            int const dst_w = std::max(1, dst_size.width);
            int const dst_h = std::max(1, dst_size.height);

            // Create source binary image
            std::vector<uint8_t> src_binary(static_cast<size_t>(src_w) * static_cast<size_t>(src_h), 0);
            for (auto const & p : src_mask) {
                if (p.x < static_cast<uint32_t>(src_w) && p.y < static_cast<uint32_t>(src_h)) {
                    src_binary[static_cast<size_t>(p.y) * static_cast<size_t>(src_w) + static_cast<size_t>(p.x)] = 255;
                }
            }

            // Endpoint-preserving dst->src nearest-neighbor mapping
            double const rx = (dst_w > 1 && src_w > 1) ? (static_cast<double>(src_w - 1) / static_cast<double>(dst_w - 1)) : 0.0;
            double const ry = (dst_h > 1 && src_h > 1) ? (static_cast<double>(src_h - 1) / static_cast<double>(dst_h - 1)) : 0.0;

            std::vector<uint8_t> dst_binary(static_cast<size_t>(dst_w) * static_cast<size_t>(dst_h), 0);
            for (int y = 0; y < dst_h; ++y) {
                int const ys = (dst_h > 1 && src_h > 1) ? static_cast<int>(std::round(static_cast<double>(y) * ry)) : 0;
                for (int x = 0; x < dst_w; ++x) {
                    int const xs = (dst_w > 1 && src_w > 1) ? static_cast<int>(std::round(static_cast<double>(x) * rx)) : 0;
                    size_t const src_index = static_cast<size_t>(ys) * static_cast<size_t>(src_w) + static_cast<size_t>(xs);
                    size_t const dst_index = static_cast<size_t>(y) * static_cast<size_t>(dst_w) + static_cast<size_t>(x);
                    if (src_index < src_binary.size() && src_binary[src_index]) {
                        dst_binary[dst_index] = 255;
                    }
                }
            }

            // Convert dst_binary back to Mask2D
            Mask2D scaled_mask;
            size_t count_pixels = 0;
            for (auto v : dst_binary) if (v) ++count_pixels;
            scaled_mask.reserve(count_pixels);
            for (int y = 0; y < dst_h; ++y) {
                for (int x = 0; x < dst_w; ++x) {
                    size_t const idx = static_cast<size_t>(y) * static_cast<size_t>(dst_w) + static_cast<size_t>(x);
                    if (dst_binary[idx]) scaled_mask.push_back(Point2D<uint32_t>{static_cast<uint32_t>(x), static_cast<uint32_t>(y)});
                }
            }

            target.addAtTime(time, scaled_mask, NotifyObservers::No);
        }

        ++count;
    }

    if (notify == NotifyObservers::Yes && count > 0) target.notifyObservers();
    return count;
}

std::size_t MaskData::moveByEntityIds(MaskData & target,
                                      std::unordered_set<EntityId> const & entity_ids,
                                      NotifyObservers notify) {
    std::vector<std::tuple<TimeFrameIndex, Mask2D, EntityId>> to_move;
    to_move.reserve(entity_ids.size());

    for (size_t i = 0; i < _storage.size(); ++i) {
        EntityId const eid = _storage.getEntityId(i);
        if (entity_ids.contains(eid)) {
            to_move.emplace_back(_storage.getTime(i), _storage.getData(i), eid);
        }
    }

    if (to_move.empty()) return 0;

    ImageSize const src_size = this->getImageSize();
    ImageSize const dst_size = target.getImageSize();

    bool need_scale = (src_size.width > 0 && src_size.height > 0 &&
                       dst_size.width > 0 && dst_size.height > 0 &&
                       (src_size.width != dst_size.width || src_size.height != dst_size.height));

    for (auto const & [time, data, eid] : to_move) {
        if (!need_scale) {
            target.addEntryAtTime(time, data, eid, NotifyObservers::No);
        } else {
            int const src_w = std::max(1, src_size.width);
            int const src_h = std::max(1, src_size.height);
            int const dst_w = std::max(1, dst_size.width);
            int const dst_h = std::max(1, dst_size.height);

            std::vector<uint8_t> src_binary(static_cast<size_t>(src_w) * static_cast<size_t>(src_h), 0);
            for (auto const & p : data) {
                if (p.x < static_cast<uint32_t>(src_w) && p.y < static_cast<uint32_t>(src_h)) {
                    src_binary[static_cast<size_t>(p.y) * static_cast<size_t>(src_w) + static_cast<size_t>(p.x)] = 255;
                }
            }

            double const rx = (dst_w > 1 && src_w > 1) ? (static_cast<double>(src_w - 1) / static_cast<double>(dst_w - 1)) : 0.0;
            double const ry = (dst_h > 1 && src_h > 1) ? (static_cast<double>(src_h - 1) / static_cast<double>(dst_h - 1)) : 0.0;

            std::vector<uint8_t> dst_binary(static_cast<size_t>(dst_w) * static_cast<size_t>(dst_h), 0);
            for (int y = 0; y < dst_h; ++y) {
                int const ys = (dst_h > 1 && src_h > 1) ? static_cast<int>(std::round(static_cast<double>(y) * ry)) : 0;
                for (int x = 0; x < dst_w; ++x) {
                    int const xs = (dst_w > 1 && src_w > 1) ? static_cast<int>(std::round(static_cast<double>(x) * rx)) : 0;
                    size_t const src_index = static_cast<size_t>(ys) * static_cast<size_t>(src_w) + static_cast<size_t>(xs);
                    size_t const dst_index = static_cast<size_t>(y) * static_cast<size_t>(dst_w) + static_cast<size_t>(x);
                    if (src_index < src_binary.size() && src_binary[src_index]) dst_binary[dst_index] = 255;
                }
            }

            Mask2D scaled_mask;
            size_t count_pixels = 0;
            for (auto v : dst_binary) if (v) ++count_pixels;
            scaled_mask.reserve(count_pixels);
            for (int y = 0; y < dst_h; ++y) {
                for (int x = 0; x < dst_w; ++x) {
                    size_t const idx = static_cast<size_t>(y) * static_cast<size_t>(dst_w) + static_cast<size_t>(x);
                    if (dst_binary[idx]) scaled_mask.push_back(Point2D<uint32_t>{static_cast<uint32_t>(x), static_cast<uint32_t>(y)});
                }
            }

            target.addEntryAtTime(time, scaled_mask, eid, NotifyObservers::No);
        }
    }

    // Remove moved entries from source storage
    for (auto const & [time, data, eid] : to_move) {
        (void)time; (void)data;
        _storage.removeByEntityId(eid);
    }

    if (notify == NotifyObservers::Yes) {
        target.notifyObservers();
        notifyObservers();
    }

    return to_move.size();
}
