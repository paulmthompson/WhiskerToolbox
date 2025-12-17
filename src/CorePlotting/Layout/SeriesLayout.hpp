#ifndef COREPLOTTING_LAYOUT_SERIESLAYOUT_HPP
#define COREPLOTTING_LAYOUT_SERIESLAYOUT_HPP

#include "LayoutTransform.hpp"
#include <string>

namespace CorePlotting {

/**
 * @brief Complete layout specification for a single series
 * 
 * Contains the transforms that position and scale data in world space.
 * The LayoutEngine computes these based on viewport and series configuration.
 * 
 * The y_transform combines:
 * - Data normalization (z-score, peak-to-peak, etc.) - from NormalizationHelpers
 * - Layout positioning (vertical stacking) - from LayoutEngine
 * - User adjustments (manual gain/offset) - from widget config
 * 
 * For time-series plots, x_transform is typically identity since the
 * TimeFrame handles X-axis mapping.
 */
struct SeriesLayout {
    /// Series identifier (e.g., key in DataManager)
    std::string series_id;
    
    /// Y-axis transform: data normalization + vertical positioning
    LayoutTransform y_transform;
    
    /// X-axis transform: usually identity for time-series
    LayoutTransform x_transform;
    
    /// Index in the layout sequence (for ordering)
    int series_index{0};
    
    /**
     * @brief Default constructor (identity transforms)
     */
    SeriesLayout() = default;
    
    /**
     * @brief Construct with series ID and Y transform
     * @param id Series identifier
     * @param y_xform Y-axis transform
     * @param index Index in layout sequence
     */
    SeriesLayout(std::string id, LayoutTransform y_xform, int index = 0)
        : series_id(std::move(id))
        , y_transform(y_xform)
        , x_transform(LayoutTransform::identity())
        , series_index(index) {}
    
    /**
     * @brief Construct with both transforms
     * @param id Series identifier
     * @param y_xform Y-axis transform
     * @param x_xform X-axis transform
     * @param index Index in layout sequence
     */
    SeriesLayout(std::string id, LayoutTransform y_xform, LayoutTransform x_xform, int index = 0)
        : series_id(std::move(id))
        , y_transform(y_xform)
        , x_transform(x_xform)
        , series_index(index) {}
    
    /**
     * @brief Compute Model matrix from transforms
     * 
     * Combines X and Y transforms into a single 4x4 matrix.
     * For time-series (x_transform is identity), this is just the Y transform.
     */
    [[nodiscard]] glm::mat4 computeModelMatrix() const {
        if (x_transform.isIdentity()) {
            return y_transform.toModelMatrixY();
        }
        // Combine X and Y transforms
        glm::mat4 m(1.0f);
        m[0][0] = x_transform.gain;    // X scale
        m[1][1] = y_transform.gain;    // Y scale
        m[3][0] = x_transform.offset;  // X translation
        m[3][1] = y_transform.offset;  // Y translation
        return m;
    }
    
    /**
     * @brief Apply Y transform to a data value
     */
    [[nodiscard]] float transformY(float data_value) const {
        return y_transform.apply(data_value);
    }
    
    /**
     * @brief Inverse Y transform (world Y → data value)
     */
    [[nodiscard]] float inverseTransformY(float world_y) const {
        return y_transform.inverse(world_y);
    }
};

} // namespace CorePlotting

#endif // COREPLOTTING_LAYOUT_SERIESLAYOUT_HPP
