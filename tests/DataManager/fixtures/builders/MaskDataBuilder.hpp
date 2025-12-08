#ifndef MASK_DATA_BUILDER_HPP
#define MASK_DATA_BUILDER_HPP

#include "Masks/Mask_Data.hpp"
#include "CoreGeometry/masks.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <memory>
#include <vector>
#include <cmath>

/**
 * @brief Helper functions for creating common mask shapes
 */
namespace mask_shapes {

/**
 * @brief Create a rectangular mask
 * @param x Starting x coordinate
 * @param y Starting y coordinate
 * @param width Width of rectangle
 * @param height Height of rectangle
 */
inline Mask2D box(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    std::vector<uint32_t> xs, ys;
    for (uint32_t dy = 0; dy < height; ++dy) {
        for (uint32_t dx = 0; dx < width; ++dx) {
            xs.push_back(x + dx);
            ys.push_back(y + dy);
        }
    }
    return Mask2D(xs, ys);
}

/**
 * @brief Create a circular mask
 * @param center_x Center x coordinate
 * @param center_y Center y coordinate
 * @param radius Radius of circle
 */
inline Mask2D circle(uint32_t center_x, uint32_t center_y, uint32_t radius) {
    std::vector<uint32_t> xs, ys;
    int r_sq = static_cast<int>(radius * radius);
    
    for (int dy = -static_cast<int>(radius); dy <= static_cast<int>(radius); ++dy) {
        for (int dx = -static_cast<int>(radius); dx <= static_cast<int>(radius); ++dx) {
            if (dx * dx + dy * dy <= r_sq) {
                xs.push_back(static_cast<uint32_t>(static_cast<int>(center_x) + dx));
                ys.push_back(static_cast<uint32_t>(static_cast<int>(center_y) + dy));
            }
        }
    }
    return Mask2D(xs, ys);
}

/**
 * @brief Create a line mask (1 pixel wide)
 * @param x1 Start x
 * @param y1 Start y
 * @param x2 End x
 * @param y2 End y
 */
inline Mask2D line(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2) {
    std::vector<uint32_t> xs, ys;
    
    int dx = static_cast<int>(x2) - static_cast<int>(x1);
    int dy = static_cast<int>(y2) - static_cast<int>(y1);
    int steps = std::max(std::abs(dx), std::abs(dy));
    
    if (steps == 0) {
        xs.push_back(x1);
        ys.push_back(y1);
        return Mask2D(xs, ys);
    }
    
    float x_step = static_cast<float>(dx) / static_cast<float>(steps);
    float y_step = static_cast<float>(dy) / static_cast<float>(steps);
    
    for (int i = 0; i <= steps; ++i) {
        xs.push_back(static_cast<uint32_t>(static_cast<int>(x1) + static_cast<int>(x_step * i)));
        ys.push_back(static_cast<uint32_t>(static_cast<int>(y1) + static_cast<int>(y_step * i)));
    }
    
    return Mask2D(xs, ys);
}

/**
 * @brief Create a single-pixel mask
 * @param x X coordinate
 * @param y Y coordinate
 */
inline Mask2D point(uint32_t x, uint32_t y) {
    std::vector<uint32_t> xs = {x};
    std::vector<uint32_t> ys = {y};
    return Mask2D(xs, ys);
}

/**
 * @brief Create an empty mask
 */
inline Mask2D empty() {
    std::vector<uint32_t> xs;
    std::vector<uint32_t> ys;
    return Mask2D(xs, ys);
}

} // namespace mask_shapes

/**
 * @brief Lightweight builder for MaskData objects
 * 
 * Provides fluent API for constructing MaskData test data with common
 * mask shapes, without requiring DataManager.
 * 
 * @example Simple box mask
 * @code
 * auto mask_data = MaskDataBuilder()
 *     .atTime(0, mask_shapes::box(10, 10, 20, 20))
 *     .build();
 * @endcode
 * 
 * @example Multiple masks at one time
 * @code
 * auto mask_data = MaskDataBuilder()
 *     .atTime(0, mask_shapes::box(0, 0, 10, 10))
 *     .atTime(0, mask_shapes::circle(50, 50, 20))
 *     .build();
 * @endcode
 */
class MaskDataBuilder {
public:
    MaskDataBuilder() = default;

    /**
     * @brief Add a mask at a specific time
     * @param time Time index
     * @param mask Mask2D to add
     */
    MaskDataBuilder& atTime(int time, const Mask2D& mask) {
        m_masks_by_time[TimeFrameIndex(time)].push_back(mask);
        return *this;
    }

    /**
     * @brief Add a mask at a specific time (TimeFrameIndex version)
     * @param time Time index
     * @param mask Mask2D to add
     */
    MaskDataBuilder& atTime(TimeFrameIndex time, const Mask2D& mask) {
        m_masks_by_time[time].push_back(mask);
        return *this;
    }

    /**
     * @brief Add multiple masks at a specific time
     * @param time Time index
     * @param masks Vector of masks to add
     */
    MaskDataBuilder& atTime(int time, const std::vector<Mask2D>& masks) {
        auto& vec = m_masks_by_time[TimeFrameIndex(time)];
        vec.insert(vec.end(), masks.begin(), masks.end());
        return *this;
    }

    /**
     * @brief Add a box mask at a specific time
     * @param time Time index
     * @param x Starting x coordinate
     * @param y Starting y coordinate
     * @param width Width of rectangle
     * @param height Height of rectangle
     */
    MaskDataBuilder& withBox(int time, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
        return atTime(time, mask_shapes::box(x, y, width, height));
    }

    /**
     * @brief Add a circular mask at a specific time
     * @param time Time index
     * @param center_x Center x coordinate
     * @param center_y Center y coordinate
     * @param radius Radius of circle
     */
    MaskDataBuilder& withCircle(int time, uint32_t center_x, uint32_t center_y, uint32_t radius) {
        return atTime(time, mask_shapes::circle(center_x, center_y, radius));
    }

    /**
     * @brief Add a point mask at a specific time
     * @param time Time index
     * @param x X coordinate
     * @param y Y coordinate
     */
    MaskDataBuilder& withPoint(int time, uint32_t x, uint32_t y) {
        return atTime(time, mask_shapes::point(x, y));
    }

    /**
     * @brief Add an empty mask at a specific time
     * @param time Time index
     */
    MaskDataBuilder& withEmpty(int time) {
        return atTime(time, mask_shapes::empty());
    }

    /**
     * @brief Set image size for the mask data
     * @param width Image width
     * @param height Image height
     */
    MaskDataBuilder& withImageSize(uint32_t width, uint32_t height) {
        m_image_width = width;
        m_image_height = height;
        m_has_image_size = true;
        return *this;
    }

    /**
     * @brief Build the MaskData
     * @return Shared pointer to constructed MaskData
     */
    std::shared_ptr<MaskData> build() const {
        auto mask_data = std::make_shared<MaskData>();
        
        for (const auto& [time, masks] : m_masks_by_time) {
            for (const auto& mask : masks) {
                mask_data->addAtTime(time, mask, NotifyObservers::No);
            }
        }
        
        if (m_has_image_size) {
            mask_data->setImageSize(ImageSize(m_image_width, m_image_height));
        }
        
        return mask_data;
    }

private:
    std::map<TimeFrameIndex, std::vector<Mask2D>> m_masks_by_time;
    uint32_t m_image_width = 800;
    uint32_t m_image_height = 600;
    bool m_has_image_size = false;
};

#endif // MASK_DATA_BUILDER_HPP
