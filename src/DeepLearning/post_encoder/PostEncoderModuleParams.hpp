/**
 * @file PostEncoderModuleParams.hpp
 * @brief User-configurable parameter structs for post-encoder modules.
 */

#ifndef NEURALYZER_POST_ENCODER_MODULE_PARAMS_HPP
#define NEURALYZER_POST_ENCODER_MODULE_PARAMS_HPP

#include <string>

namespace dl {

/**
 * @brief Interpolation strategy for spatial feature extraction.
 */
enum class InterpolationMode {
    /** Round to nearest grid location (fast) */
    Nearest,
    /** Sub-pixel bilinear interpolation via grid_sample (accurate) */
    Bilinear
};

/** User-configurable params for GlobalAvgPoolModule (none). */
struct GlobalAvgPoolModuleParams {};

/**
 * @brief User-configurable params for SpatialPointExtractModule.
 */
struct SpatialPointModuleParams {
    InterpolationMode interpolation = InterpolationMode::Nearest;
    /** DataManager key for PointData supplying the per-frame query point */
    std::string point_key;
};

}// namespace dl

#endif// NEURALYZER_POST_ENCODER_MODULE_PARAMS_HPP
