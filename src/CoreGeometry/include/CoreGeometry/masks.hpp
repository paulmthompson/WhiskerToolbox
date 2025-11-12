#ifndef DATAMANAGER_MASKS_HPP
#define DATAMANAGER_MASKS_HPP

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/points.hpp"

#include <cstdint>
#include <vector>

class Mask2D {
public:
    Mask2D() = default;
    
    /**
     * @brief Construct a Mask2D from a vector of Point2D<uint32_t>.
     *
     * @pre None.
     * @post The mask contains the provided points.
     */
    explicit Mask2D(std::vector<Point2D<uint32_t>> points) : points_(std::move(points)) {}
    
    /**
     * @brief Construct a Mask2D from an initializer list of Point2D<uint32_t>.
     *
     * @pre None.
     * @post The mask contains the provided points in the same order.
     */
    Mask2D(std::initializer_list<Point2D<uint32_t>> points) : points_(points) {}
    
    /**
     * @brief Construct a Mask2D from separate x and y coordinate vectors (uint32_t).
     *
     * @pre x and y vectors must have the same size.
     * @post The mask contains points created from the x and y coordinates.
     */
    Mask2D(std::vector<uint32_t> const & x, std::vector<uint32_t> const & y);
    
    /**
     * @brief Construct a Mask2D from separate x and y coordinate vectors (float).
     *
     * Float coordinates are rounded to nearest integers and clamped to non-negative values.
     *
     * @pre x and y vectors must have the same size.
     * @post The mask contains points created from the rounded x and y coordinates.
     */
    Mask2D(std::vector<float> const & x, std::vector<float> const & y);

    size_t size() const {
        return points_.size();
    }

    Point2D<uint32_t> front() const {
        return points_.front();
    }

    bool empty() const {
        return points_.empty();
    }

    void push_back(Point2D<uint32_t> const & point) {
        points_.push_back(point);
    }

    void reserve(size_t capacity) {
        points_.reserve(capacity);
    }

    Point2D<uint32_t> back() const {
        return points_.back();
    }
    
    Point2D<uint32_t> operator[](size_t index) const {
        return points_[index];
    }

    std::vector<Point2D<uint32_t>>::iterator begin() {
        return points_.begin();
    }

    std::vector<Point2D<uint32_t>>::iterator end() {
        return points_.end();
    }
    
    std::vector<Point2D<uint32_t>>::const_iterator begin() const {
        return points_.begin();
    }
    
    std::vector<Point2D<uint32_t>>::const_iterator end() const {
        return points_.end();
    }

    void erase(std::vector<Point2D<uint32_t>>::iterator it) {
        points_.erase(it);
    }

    void erase(std::vector<Point2D<uint32_t>>::iterator first, std::vector<Point2D<uint32_t>>::iterator last) {
        points_.erase(first, last);
    }

    /**
     * @brief Get a const reference to the underlying vector of points
     *
     * @return const reference to the vector of points
     */
    std::vector<Point2D<uint32_t>> const & points() const {
        return points_;
    }

private:
    std::vector<Point2D<uint32_t>> points_;
};


std::pair<Point2D<uint32_t>, Point2D<uint32_t>> get_bounding_box(Mask2D const & mask);

/**
 * @brief Compute the outline of a mask by finding extremal points
 *
 * This function creates an outline by finding:
 * - For each unique x coordinate, the maximum y value
 * - For each unique y coordinate, the maximum x value
 * These extremal points are then connected to form the boundary outline.
 *
 * @param mask The input mask (vector of Point2D)
 * @return Line2D containing the outline points
 *
 * @note If mask is empty or has only one point, returns empty outline
 */
std::vector<Point2D<uint32_t>> get_mask_outline(Mask2D const & mask);

/**
 * @brief Generate mask pixels within an elliptical region
 *
 * Generates all pixels that fall within an elliptical region centered at the given point.
 * When radius_x == radius_y, this creates a perfect circle.
 * Input coordinates are rounded to nearest integers for pixel precision.
 *
 * @param center_x X coordinate of the ellipse center
 * @param center_y Y coordinate of the ellipse center  
 * @param radius_x Radius in the X direction
 * @param radius_y Radius in the Y direction
 * @return Vector of Point2D containing all pixels within the ellipse
 *
 * @note Only returns pixels with non-negative coordinates
 * @note Uses the standard ellipse equation: (dx/radius_x)² + (dy/radius_y)² ≤ 1
 */
std::vector<Point2D<uint32_t>> generate_ellipse_pixels(float center_x, float center_y, float radius_x, float radius_y);

/**
 * @brief Combine two masks using union operation (no duplicate pixels)
 *
 * Takes two masks and returns a new mask containing all pixels from both masks,
 * with duplicates removed. Pixels are considered duplicates if they have the
 * same rounded integer coordinates.
 *
 * @param mask1 First mask
 * @param mask2 Second mask to union with the first
 * @return New mask containing union of both input masks
 */
Mask2D combine_masks(Mask2D const & mask1, Mask2D const & mask2);

/**
 * @brief Subtract mask2 from mask1 (remove intersecting pixels)
 *
 * Takes two masks and returns a new mask containing only pixels from mask1
 * that are NOT present in mask2. Pixels are considered equal if they have
 * the same rounded integer coordinates.
 *
 * @param mask1 Base mask to subtract from
 * @param mask2 Mask containing pixels to remove from mask1
 * @return New mask with mask2 pixels removed from mask1
 */
Mask2D subtract_masks(Mask2D const & mask1, Mask2D const & mask2);

/**
 * @brief Generate an outline mask from an existing mask
 *
 * Creates a new mask that represents the outline/border of the input mask.
 * The outline is created by including pixels that are on the edge of the mask
 * (pixels that have at least one neighbor that is not in the original mask).
 *
 * @param mask The input mask to generate outline from
 * @param thickness The thickness of the outline in pixels (default: 1)
 * @param image_width Width of the image bounds for edge detection
 * @param image_height Height of the image bounds for edge detection
 * @return New mask containing only the outline pixels
 */
Mask2D generate_outline_mask(Mask2D const & mask, int thickness = 1, uint32_t image_width = UINT32_MAX, uint32_t image_height = UINT32_MAX);



std::vector<Point2D<uint32_t>> extract_line_pixels(
        std::vector<uint8_t> const & binary_img,
        ImageSize const image_size);


#endif// DATAMANAGER_MASKS_HPP
