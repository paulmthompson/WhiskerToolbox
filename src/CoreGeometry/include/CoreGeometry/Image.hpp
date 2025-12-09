#ifndef WHISKERTOOLBOX_IMAGE_HPP
#define WHISKERTOOLBOX_IMAGE_HPP

#include "ImageSize.hpp"
#include "points.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

/**
 * @brief Get pixel value from image data at a specified point
 * 
 * This template function retrieves the pixel value at a given point in an image,
 * using row-major order indexing. The point coordinates are rounded to the nearest
 * integer pixel location.
 * 
 * @tparam T The pixel data type (e.g., uint8_t, float)
 * @param point The point coordinates to sample
 * @param image_data The image pixel data in row-major order
 * @param image_size The dimensions of the image
 * @return The pixel value at the specified point, or 0 if out of bounds
 */
template<typename T>
T get_pixel_value(Point2D<float> const & point,
                  std::vector<T> const & image_data,
                  ImageSize const & image_size) {
    int x = static_cast<int>(std::round(point.x));
    int y = static_cast<int>(std::round(point.y));

    // Check bounds
    if (x < 0 || x >= image_size.width || y < 0 || y >= image_size.height) {
        return 0;
    }

    size_t index = static_cast<size_t>(y * image_size.width + x);
    if (index < image_data.size()) {
        return image_data[index];
    }

    return 0;
}

/**
 * @brief A binary image structure containing pixel data and dimensions.
 * 
 * This struct encapsulates a binary image represented as a 1D vector of uint8_t values
 * along with its dimensions. The pixel data is stored in row-major order, meaning
 * that pixels are stored row by row, with each row's pixels stored contiguously.
 * 
 * For an image with width W and height H, the pixel at position (row, col) 
 * can be accessed at index: row * width + col
 * 
 * @note Data Layout: ROW-MAJOR ORDER
 *       - Memory layout: [row0_col0, row0_col1, ..., row0_colW-1, row1_col0, row1_col1, ...]
 *       - Index formula: pixel(row, col) = data[row * width + col]
 *       - Iterating through data linearly processes all columns of row 0, then row 1, etc.
 */
struct Image {
    std::vector<uint8_t> data;///< Pixel data in row-major order (0 = background, non-zero = foreground)
    ImageSize size;           ///< Image dimensions (width and height)

    /**
     * @brief Default constructor creates an empty image.
     */
    Image() = default;

    /**
     * @brief Constructs an image with specified data and size.
     * 
     * @param pixel_data Image pixel data in row-major order
     * @param image_size Image dimensions
     * 
     * @pre pixel_data.size() must equal image_size.width * image_size.height
     * @pre image_size.width and image_size.height must be greater than 0
     */
    Image(std::vector<uint8_t> pixel_data, ImageSize image_size)
        : data(std::move(pixel_data)),
          size(image_size) {}

    /**
     * @brief Constructs an image with specified dimensions, initialized to zero.
     * 
     * @param image_size Image dimensions
     * 
     * @pre image_size.width and image_size.height must be greater than 0
     */
    explicit Image(ImageSize image_size)
        : data(static_cast<size_t>(image_size.width * image_size.height), 0),
          size(image_size) {}

    /**
     * @brief Gets the pixel value at the specified coordinates.
     * 
     * @param row Row index (0-based)
     * @param col Column index (0-based)
     * @return uint8_t Pixel value at (row, col)
     * 
     * @pre row must be in range [0, size.height)
     * @pre col must be in range [0, size.width)
     */
    uint8_t at(int row, int col) const {
        return data[static_cast<size_t>(row * size.width + col)];
    }

    /**
     * @brief Sets the pixel value at the specified coordinates.
     * 
     * @param row Row index (0-based)
     * @param col Column index (0-based)
     * @param value Pixel value to set
     * 
     * @pre row must be in range [0, size.height)
     * @pre col must be in range [0, size.width)
     */
    void set(int row, int col, uint8_t value) {
        data[static_cast<size_t>(row * size.width + col)] = value;
    }

    /**
     * @brief Gets the total number of pixels in the image.
     * 
     * @return size_t Total pixel count (width * height), or 0 if image is empty
     */
    [[nodiscard]]size_t pixel_count() const {
        if (empty()) {
            return 0;
        }
        return static_cast<size_t>(size.width * size.height);
    }

    /**
     * @brief Checks if the image is empty (no pixels).
     * 
     * @return bool True if the image has no pixels, false otherwise
     */
    [[nodiscard]] bool empty() const {
        return size.width <= 0 || size.height <= 0 || data.empty();
    }
};

#endif// WHISKERTOOLBOX_IMAGE_HPP