#ifndef DATAMANAGER_MASKS_HPP
#define DATAMANAGER_MASKS_HPP

#include "Points/points.hpp"

#include <vector>

using Mask2D = std::vector<Point2D<float>>;


inline Mask2D create_mask(std::vector<float> const& x, std::vector<float> const& y)
{
    auto new_mask = Mask2D{x.size()};

    for (std::size_t i = 0; i < x.size(); i++)
    {
        new_mask[i] = Point2D<float>{x[i],y[i]};
    }

    return new_mask;
}


#endif // DATAMANAGER_MASKS_HPP
