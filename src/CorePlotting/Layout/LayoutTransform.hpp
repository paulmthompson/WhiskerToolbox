#ifndef COREPLOTTING_LAYOUT_LAYOUTTRANSFORM_HPP
#define COREPLOTTING_LAYOUT_LAYOUTTRANSFORM_HPP

#include <glm/glm.hpp>

#include <cmath>

namespace CorePlotting {

/**
 * @brief Pure geometric transform: output = input * gain + offset
 * 
 * This is the fundamental building block for positioning data in world space.
 * The LayoutEngine outputs these, and they become part of the Model matrix.
 * 
 * The transform is applied as: transformed_value = raw_value * gain + offset
 * 
 * This simple abstraction allows:
 * - Data normalization (z-score, peak-to-peak, etc.)
 * - Layout positioning (vertical stacking)
 * - User adjustments (manual gain/offset tweaks)
 * 
 * Transforms can be composed: applying A then B is equivalent to A.compose(B).
 */
struct LayoutTransform {
    float offset{0.0f};///< Translation (applied after scaling)
    float gain{1.0f};  ///< Scale factor

    /**
     * @brief Default constructor (identity transform)
     */
    LayoutTransform() = default;

    /**
     * @brief Construct with offset and gain
     * @param off Offset (translation)
     * @param g Gain (scale factor)
     */
    LayoutTransform(float off, float g)
        : offset(off),
          gain(g) {}

    /**
     * @brief Apply transform: output = input * gain + offset
     */
    [[nodiscard]] float apply(float value) const {
        return value * gain + offset;
    }

    /**
     * @brief Inverse transform: recover original value from transformed
     * 
     * @note Returns 0 if gain is effectively zero (non-invertible)
     */
    [[nodiscard]] float inverse(float transformed) const {
        if (std::abs(gain) < 1e-10f) return 0.0f;
        return (transformed - offset) / gain;
    }

    /**
     * @brief Compose transforms: result applies this AFTER other
     * 
     * If we apply `other` then `this`:
     *   result = (x * other.gain + other.offset) * this.gain + this.offset
     *          = x * (other.gain * this.gain) + (other.offset * this.gain + this.offset)
     * 
     * @param other Transform applied first
     * @return Combined transform equivalent to applying other then this
     */
    [[nodiscard]] LayoutTransform compose(LayoutTransform const & other) const {
        return {other.offset * gain + offset, other.gain * gain};
    }

    /**
     * @brief Convert to 4x4 Model matrix for Y-axis transform
     * 
     * Creates a matrix that applies this transform to Y coordinates only.
     * X and Z coordinates pass through unchanged.
     */
    [[nodiscard]] glm::mat4 toModelMatrixY() const {
        glm::mat4 m(1.0f);
        m[1][1] = gain;  // Y scale
        m[3][1] = offset;// Y translation
        return m;
    }

    /**
     * @brief Convert to 4x4 Model matrix for X-axis transform
     * 
     * Creates a matrix that applies this transform to X coordinates only.
     */
    [[nodiscard]] glm::mat4 toModelMatrixX() const {
        glm::mat4 m(1.0f);
        m[0][0] = gain;  // X scale
        m[3][0] = offset;// X translation
        return m;
    }

    /**
     * @brief Check if this is approximately an identity transform
     */
    [[nodiscard]] bool isIdentity(float epsilon = 1e-6f) const {
        return std::abs(offset) < epsilon && std::abs(gain - 1.0f) < epsilon;
    }

    /**
     * @brief Create identity transform
     */
    [[nodiscard]] static LayoutTransform identity() {
        return {0.0f, 1.0f};
    }
};

/**
 * @brief Equality comparison
 */
inline bool operator==(LayoutTransform const & a, LayoutTransform const & b) {
    return a.offset == b.offset && a.gain == b.gain;
}

inline bool operator!=(LayoutTransform const & a, LayoutTransform const & b) {
    return !(a == b);
}

}// namespace CorePlotting

#endif// COREPLOTTING_LAYOUT_LAYOUTTRANSFORM_HPP
