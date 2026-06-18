#ifndef COREGEOMETRY_ANGLE_HPP
#define COREGEOMETRY_ANGLE_HPP

class Line2D;

/**
 * @brief Fractional arc-length span along a polyline (0 = start, 1 = end of total length).
 */
struct ArcLengthFractionSpan {
    float t_low = 0.0f;
    float t_high = 1.0f;
};

/**
 * @brief Orthonormal right-handed basis in the plane ex, ey (column vectors as components).
 */
struct PlanarOrthonormalBasis2D {
    float ex_x = 1.0f;
    float ex_y = 0.0f;
    float ey_x = 0.0f;
    float ey_y = 1.0f;
};

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
 * @brief Symmetric window around midpoint, slid so it lies in [0, 1] (fractional arc length).
 *
 * Nominal interval is [position - window/2, position + window/2], then shifted into [0, 1].
 * If window >= 1, returns [0, 1]. If the span collapses, expands a small neighborhood around
 * position so t_low < t_high.
 *
 * @param position Midpoint fraction in [0, 1] (clamped)
 * @param window Full width of the window in [0, 1] (clamped)
 */
[[nodiscard]] ArcLengthFractionSpan compute_sliding_secant_span(float position, float window);

/**
 * @brief Build an orthonormal right-handed (ê_x, ê_y) from user positive X and positive Y directions.
 *
 * Normalizes the X direction, Gram–Schmidt orthogonalizes Y onto X, then normalizes Y.
 * If X is degenerate, uses (1, 0). If Y is parallel to X after projection, uses (-ê_x_y, ê_x_x).
 * If the resulting pair is left-handed, flips ê_y so cross(ê_x, ê_y) > 0.
 */
[[nodiscard]] PlanarOrthonormalBasis2D make_planar_orthonormal_basis(float axis_positive_x_x,
                                                                    float axis_positive_x_y,
                                                                    float axis_positive_y_x,
                                                                    float axis_positive_y_y);

/**
 * @brief Angle in degrees of world vector (vx, vy) expressed in basis (atan2 in ê_x, ê_y frame).
 *
 * @post Result wrapped to (-180, 180]
 */
[[nodiscard]] float angle_degrees_vector_in_basis(float vx, float vy, PlanarOrthonormalBasis2D const & basis);

/**
 * @brief Secant angle between interpolated points at t_low and t_high, in the given basis.
 *
 * Uses cumulative arc-length fractions and linear interpolation along polyline segments
 * (see point_at_fractional_position with interpolation enabled).
 *
 * @pre line must have at least 2 points (enforcement: returns 0.0f otherwise)
 */
[[nodiscard]] float calculate_direct_angle_secant(Line2D const & line,
                                                  float t_low,
                                                  float t_high,
                                                  PlanarOrthonormalBasis2D const & basis);


#endif// COREGEOMETRY_ANGLE_HPP
