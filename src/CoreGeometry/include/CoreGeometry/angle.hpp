#ifndef COREGEOMETRY_ANGLE_HPP
#define COREGEOMETRY_ANGLE_HPP

class Line2D;

float normalize_angle(float raw_angle, float reference_x, float reference_y);

float calculate_direct_angle(Line2D const & line, float position, float reference_x, float reference_y);


#endif // COREGEOMETRY_ANGLE_HPP