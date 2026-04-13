#ifndef COREGEOMETRY_ANGLE_HPP
#define COREGEOMETRY_ANGLE_HPP

class Line2D;

/**
 * @brief Normalize an angle relative to a reference vector direction
 *
 * Subtracts the reference vector's angle from raw_angle and wraps the result
 * into the range (-180, 180].
 *
 * @param raw_angle Angle in degrees
 * @param reference_x X component of the reference direction vector
 * @param reference_y Y component of the reference direction vector
 * @return Normalized angle in degrees, in (-180, 180]
 *
 * @pre raw_angle must be finite (not NaN or Inf) (enforcement: none) [IMPORTANT]
 * @pre reference_x and reference_y must not both be zero (enforcement: none) [LOW]
 */
float normalize_angle(float raw_angle, float reference_x, float reference_y);

/**
 * @brief Calculate the angle from the base (first point) to an interpolated
 *        position along a line
 *
 * The position is specified as a fractional distance along the line's cumulative
 * arc length (0.0 = start, 1.0 = end). The point at that position is obtained
 * by linear interpolation along the line segments, so that the result varies
 * smoothly with the position parameter.
 *
 * @param line       The line to calculate the angle on
 * @param position   Fractional position along the line (0.0 to 1.0)
 * @param reference_x X component of the reference direction vector
 * @param reference_y Y component of the reference direction vector
 * @return Angle in degrees in (-180, 180], or 0.0f if line has fewer than 2 points
 *
 * @pre line must have at least 2 points for a meaningful result; returns 0.0f otherwise (enforcement: runtime_check)
 * @pre position should be in [0.0, 1.0]; values outside are clamped (enforcement: runtime_check)
 * @pre reference_x and reference_y should not both be zero (enforcement: none) [LOW]
 */
float calculate_direct_angle(Line2D const & line, float position, float reference_x, float reference_y);


#endif// COREGEOMETRY_ANGLE_HPP