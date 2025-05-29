#ifndef DATAMANAGER_MASKS_HPP
#define DATAMANAGER_MASKS_HPP

#include "Points/points.hpp"

#include <vector>

using Mask2D = std::vector<Point2D<float>>;


Mask2D create_mask(std::vector<float> const& x, std::vector<float> const& y);

// Optimized version that moves the input vectors
Mask2D create_mask(std::vector<float> && x, std::vector<float> && y);

std::pair<Point2D<float>,Point2D<float>> get_bounding_box(Mask2D const& mask);

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
std::vector<Point2D<float>> get_mask_outline(Mask2D const& mask);


#endif // DATAMANAGER_MASKS_HPP
