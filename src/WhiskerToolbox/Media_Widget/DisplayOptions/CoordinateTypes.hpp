#ifndef COORDINATE_TYPES_HPP
#define COORDINATE_TYPES_HPP

/**
 * @brief Strong types for different coordinate systems used in the media display
 * 
 * These types prevent confusion between canvas coordinates (display pixels),
 * media coordinates (source image pixels), and other coordinate systems.
 */

/**
 * @brief Represents a point in canvas coordinate space (display pixels)
 * 
 * Canvas coordinates are the pixel coordinates on the displayed canvas/widget.
 * Origin (0,0) is typically at the top-left of the display.
 */
struct CanvasCoordinates {
    float x{0.0f};
    float y{0.0f};

    CanvasCoordinates() = default;
    CanvasCoordinates(float x_coord, float y_coord)
        : x(x_coord),
          y(y_coord) {}
};

/**
 * @brief Represents a point in media coordinate space (source image pixels)
 * 
 * Media coordinates are the pixel coordinates in the original source image/video.
 * These coordinates are independent of how the image is scaled or displayed.
 */
struct MediaCoordinates {
    float x{0.0f};
    float y{0.0f};

    MediaCoordinates() = default;
    MediaCoordinates(float x_coord, float y_coord)
        : x(x_coord),
          y(y_coord) {}
};

/**
 * @brief Represents a point in mask coordinate space
 * 
 * Mask coordinates are the pixel coordinates within a mask's coordinate system.
 * A mask may have different dimensions than the source media.
 */
struct MaskCoordinates {
    float x{0.0f};
    float y{0.0f};

    MaskCoordinates() = default;
    MaskCoordinates(float x_coord, float y_coord)
        : x(x_coord),
          y(y_coord) {}
};

#endif// COORDINATE_TYPES_HPP