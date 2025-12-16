#ifndef COREPLOTTING_LAYOUT_SPATIALLAYOUTSTRATEGY_HPP
#define COREPLOTTING_LAYOUT_SPATIALLAYOUTSTRATEGY_HPP

#include "LayoutEngine.hpp"
#include "CoreGeometry/boundingbox.hpp"

#include <glm/vec2.hpp>

namespace CorePlotting {

/**
 * @brief Layout result for spatial data with coordinate transforms
 * 
 * Unlike stacked layouts which only position data in Y, spatial layouts
 * transform both X and Y coordinates to fit data bounds into a viewport.
 * 
 * The transform is: output = input * scale + offset
 */
struct SpatialTransform {
    float x_scale{1.0f};   ///< X coordinate scale factor
    float y_scale{1.0f};   ///< Y coordinate scale factor
    float x_offset{0.0f};  ///< X coordinate offset (applied after scaling)
    float y_offset{0.0f};  ///< Y coordinate offset (applied after scaling)
    
    /**
     * @brief Apply transform to a point
     */
    [[nodiscard]] glm::vec2 apply(glm::vec2 const& point) const {
        return {
            point.x * x_scale + x_offset,
            point.y * y_scale + y_offset
        };
    }
    
    /**
     * @brief Apply transform to x coordinate only
     */
    [[nodiscard]] float applyX(float x) const {
        return x * x_scale + x_offset;
    }
    
    /**
     * @brief Apply transform to y coordinate only
     */
    [[nodiscard]] float applyY(float y) const {
        return y * y_scale + y_offset;
    }
    
    /**
     * @brief Create identity transform (no change)
     */
    [[nodiscard]] static SpatialTransform identity() {
        return SpatialTransform{};
    }
};

/**
 * @brief Extended layout for spatial data
 * 
 * Combines the standard SeriesLayout (for Y positioning) with a full
 * 2D spatial transform for plots where both X and Y come from data.
 */
struct SpatialSeriesLayout {
    SeriesLayout layout;           ///< Standard layout (for compatibility)
    SpatialTransform transform;    ///< 2D coordinate transform
    
    SpatialSeriesLayout() = default;
    
    SpatialSeriesLayout(SeriesLayout const& l, SpatialTransform const& t)
        : layout(l), transform(t) {}
};

/**
 * @brief Response from spatial layout computation
 */
struct SpatialLayoutResponse {
    /// Single layout (spatial plots typically have one data series)
    SpatialSeriesLayout layout;
    
    /// Data bounds used for layout (may be padded from input)
    BoundingBox effective_data_bounds{0.0f, 0.0f, 0.0f, 0.0f};
    
    /// Viewport bounds used for layout
    BoundingBox viewport_bounds{0.0f, 0.0f, 0.0f, 0.0f};
};

/**
 * @brief Spatial layout strategy (SpatialOverlay style)
 * 
 * Computes coordinate transforms to fit spatial data (points, lines, masks)
 * into a viewport. Unlike time-series layouts which only stack vertically,
 * spatial layouts transform both X and Y coordinates.
 * 
 * LAYOUT MODES:
 * 1. Fit: Scale uniformly to fit data bounds into viewport (preserves aspect ratio)
 * 2. Fill: Scale non-uniformly to fill entire viewport (may distort aspect ratio)
 * 3. Identity: No transform (1:1 mapping, useful when data is already in viewport coords)
 * 
 * COORDINATE SYSTEM:
 * - Input coordinates are in data space (e.g., image pixels, sensor coordinates)
 * - Output coordinates are in viewport space (typically -1 to +1 NDC or 0 to width/height)
 * - Padding adds margin around data bounds
 * 
 * USE CASES:
 * - SpatialOverlay: Whisker visualization, mask overlay
 * - Image annotation: Points/lines over image coordinates
 * - Direct spatial data: Sensor readings with X/Y positions
 */
class SpatialLayoutStrategy {
public:
    /**
     * @brief Layout mode for fitting data into viewport
     */
    enum class Mode {
        Fit,      ///< Uniform scale to fit (preserves aspect ratio)
        Fill,     ///< Non-uniform scale to fill (may distort)
        Identity  ///< No transform (1:1 mapping)
    };
    
    /**
     * @brief Construct with default mode (Fit)
     */
    SpatialLayoutStrategy() = default;
    
    /**
     * @brief Construct with specific mode
     */
    explicit SpatialLayoutStrategy(Mode mode) : _mode(mode) {}
    
    /**
     * @brief Compute spatial layout transform
     * 
     * Calculates the transform to map data bounds into viewport bounds.
     * 
     * @param data_bounds Bounding box of the data to display
     * @param viewport_bounds Target viewport bounds
     * @param padding Relative padding (0.0 = no padding, 0.1 = 10% padding)
     * @return Spatial layout with transform
     */
    [[nodiscard]] SpatialLayoutResponse compute(
            BoundingBox const& data_bounds,
            BoundingBox const& viewport_bounds,
            float padding = 0.0f) const;
    
    /**
     * @brief Compute layout using standard LayoutRequest interface
     * 
     * For compatibility with ILayoutStrategy interface. Uses viewport from
     * request and assumes single series with data bounds in metadata.
     * 
     * @note Prefer compute(BoundingBox, BoundingBox, float) for spatial data.
     */
    [[nodiscard]] LayoutResponse computeFromRequest(LayoutRequest const& request) const;

private:
    Mode _mode{Mode::Fit};
    
    /**
     * @brief Compute uniform scale transform (Fit mode)
     */
    [[nodiscard]] SpatialTransform computeFitTransform(
            BoundingBox const& data_bounds,
            BoundingBox const& viewport_bounds) const;
    
    /**
     * @brief Compute non-uniform scale transform (Fill mode)
     */
    [[nodiscard]] SpatialTransform computeFillTransform(
            BoundingBox const& data_bounds,
            BoundingBox const& viewport_bounds) const;
};

} // namespace CorePlotting

#endif // COREPLOTTING_LAYOUT_SPATIALLAYOUTSTRATEGY_HPP
