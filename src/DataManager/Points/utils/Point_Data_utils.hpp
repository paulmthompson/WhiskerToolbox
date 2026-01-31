#ifndef POINT_DATA_UTILS_HPP
#define POINT_DATA_UTILS_HPP

#include "CoreGeometry/boundingbox.hpp"

class PointData;

/**
* @brief Calculate bounding box for a PointData object
* @param point_data The PointData to calculate bounds for
* @return BoundingBox for the PointData
*/
BoundingBox calculateBoundsForPointData(PointData const * point_data);

#endif//POINT_DATA_UTILS_HPP
