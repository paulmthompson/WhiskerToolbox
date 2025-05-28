#ifndef DATAMANAGER_MASKS_HPP
#define DATAMANAGER_MASKS_HPP

#include "Points/points.hpp"

#include <vector>

using Mask2D = std::vector<Point2D<float>>;


inline Mask2D create_mask(std::vector<float> const& x, std::vector<float> const& y)
{
    auto new_mask = Mask2D{};
    new_mask.reserve(x.size());  // Reserve space to avoid reallocations

    for (std::size_t i = 0; i < x.size(); i++)
    {
        new_mask.emplace_back(x[i], y[i]);  // Use emplace_back for efficiency
    }

    return new_mask;
}

// Optimized version that moves the input vectors
inline Mask2D create_mask(std::vector<float> && x, std::vector<float> && y)
{
    auto new_mask = Mask2D{};
    new_mask.reserve(x.size());  // Reserve space to avoid reallocations

    for (std::size_t i = 0; i < x.size(); i++)
    {
        new_mask.emplace_back(x[i], y[i]);  // Access elements directly
    }

    return new_mask;
}

inline std::pair<Point2D<float>,Point2D<float>> get_bounding_box(Mask2D const& mask)
{
    float min_x = mask[0].x;
    float max_x = mask[0].x;
    float min_y = mask[0].y;
    float max_y = mask[0].y;

    for (auto const& point : mask)
    {
        min_x = std::min(min_x, point.x);
        max_x = std::max(max_x, point.x);
        min_y = std::min(min_y, point.y);
        max_y = std::max(max_y, point.y);
    }

    return {Point2D<float>{min_x, min_y}, Point2D<float>{max_x, max_y}};
}


#endif // DATAMANAGER_MASKS_HPP
