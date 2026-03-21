/**
 * @file CanvasCoordinateSystem.hpp
 * @brief Canvas coordinate system and scaling utilities for overlay rendering
 *
 * Provides the CanvasCoordinateSystem struct (the logical resolution overlays are
 * plotted into), CoordinateMappingMode (how per-data ImageSize relates to the canvas),
 * and a free function computeScalingFactors() that replaces the ad-hoc getXAspect/
 * getYAspect pattern in Media_Window.
 */
#ifndef CANVAS_COORDINATE_SYSTEM_HPP
#define CANVAS_COORDINATE_SYSTEM_HPP

#include "CoreGeometry/ImageSize.hpp"

#include <string>

/**
 * @brief How a data object's ImageSize relates to the canvas coordinate system
 *
 * Controls whether overlay coordinates are stretched to fill the full canvas
 * (ScaleToCanvas) or interpreted as already being in canvas-coordinate-system
 * units (SubsetOfCanvas).
 */
enum class CoordinateMappingMode {
    /// Data covers the full canvas extent at its own resolution.
    /// xAspect = canvasPixelWidth / data.imageSize.width
    ScaleToCanvas,

    /// Data occupies a spatial sub-region; coordinates are in canvas-coordinate-system
    /// units already.
    /// xAspect = canvasPixelWidth / canvasCoordSystem.width
    SubsetOfCanvas
};

/**
 * @brief Source that determined the canvas coordinate system resolution
 */
enum class CanvasCoordinateSource {
    Default,    ///< Fallback 640×480
    Media,      ///< From active MediaData
    DataObject, ///< From first data object with defined ImageSize
    UserOverride///< Explicit user setting
};

/**
 * @brief The logical resolution that overlay data coordinates are interpreted against
 *
 * This replaces the implicit "ask the active media for its resolution" pattern.
 * The canvas coordinate system defines the coordinate space; the canvas pixel
 * dimensions (_canvasWidth/_canvasHeight) define the display resolution.
 */
struct CanvasCoordinateSystem {
    int width = 640; ///< Logical width of the coordinate space
    int height = 480;///< Logical height of the coordinate space
    CanvasCoordinateSource source = CanvasCoordinateSource::Default;
    std::string source_key;///< Data key that provided this resolution (empty for Default/UserOverride)

    bool operator==(CanvasCoordinateSystem const & other) const {
        return width == other.width && height == other.height;
    }

    bool operator!=(CanvasCoordinateSystem const & other) const {
        return !(*this == other);
    }

    /// @brief Convert to ImageSize for compatibility
    [[nodiscard]] ImageSize toImageSize() const {
        return {width, height};
    }

    /// @brief Create from ImageSize
    static CanvasCoordinateSystem fromImageSize(ImageSize const & size,
                                                CanvasCoordinateSource src = CanvasCoordinateSource::Default,
                                                std::string key = {}) {
        return {size.width, size.height, src, std::move(key)};
    }
};

/**
 * @brief X and Y scaling factors for mapping data coordinates to canvas pixels
 */
struct ScalingFactors {
    float x = 1.0f;
    float y = 1.0f;
};

/**
 * @brief Compute scaling factors for mapping data coordinates to canvas pixel space
 *
 * @param canvas_pixel_width  Width of the canvas in display pixels
 * @param canvas_pixel_height Height of the canvas in display pixels
 * @param coord_system        The logical canvas coordinate system
 * @param data_image_size     The data object's own ImageSize (-1,-1 if undefined)
 * @param mapping_mode        How the data's ImageSize relates to the canvas
 * @return ScalingFactors to multiply data coordinates by
 *
 * @pre canvas_pixel_width > 0 && canvas_pixel_height > 0
 *
 * Behavior:
 * - If data_image_size is undefined (-1,-1): uses coord_system dimensions
 *   (equivalent to SubsetOfCanvas with matching dimensions)
 * - If mapping_mode == ScaleToCanvas: data coordinates span [0, data_image_size)
 *   and are stretched to fill [0, canvas_pixel_width/height)
 * - If mapping_mode == SubsetOfCanvas: data coordinates are in coord_system units
 *   and scaled the same way as the coordinate system itself
 */
inline ScalingFactors computeScalingFactors(int canvas_pixel_width,
                                            int canvas_pixel_height,
                                            CanvasCoordinateSystem const & coord_system,
                                            ImageSize const & data_image_size,
                                            CoordinateMappingMode mapping_mode) {
    ScalingFactors factors;

    if (data_image_size.isDefined() && mapping_mode == CoordinateMappingMode::ScaleToCanvas) {
        // Data fills the full canvas at its own resolution
        factors.x = static_cast<float>(canvas_pixel_width) / static_cast<float>(data_image_size.width);
        factors.y = static_cast<float>(canvas_pixel_height) / static_cast<float>(data_image_size.height);
    } else {
        // Use the canvas coordinate system (either SubsetOfCanvas or undefined ImageSize)
        factors.x = static_cast<float>(canvas_pixel_width) / static_cast<float>(coord_system.width);
        factors.y = static_cast<float>(canvas_pixel_height) / static_cast<float>(coord_system.height);
    }

    return factors;
}

/**
 * @brief Compute the inverse scaling (canvas pixels → coordinate system units)
 *
 * Used for converting mouse click positions back to data/media coordinates.
 *
 * @param canvas_pixel_width  Width of the canvas in display pixels
 * @param canvas_pixel_height Height of the canvas in display pixels
 * @param coord_system        The logical canvas coordinate system
 * @return ScalingFactors where x,y are pixels-to-coord multipliers
 */
inline ScalingFactors computeInverseScalingFactors(int canvas_pixel_width,
                                                   int canvas_pixel_height,
                                                   CanvasCoordinateSystem const & coord_system) {
    ScalingFactors factors;
    factors.x = static_cast<float>(coord_system.width) / static_cast<float>(canvas_pixel_width);
    factors.y = static_cast<float>(coord_system.height) / static_cast<float>(canvas_pixel_height);
    return factors;
}

#endif// CANVAS_COORDINATE_SYSTEM_HPP
