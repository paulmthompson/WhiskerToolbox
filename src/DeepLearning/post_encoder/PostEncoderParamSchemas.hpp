/// @file PostEncoderParamSchemas.hpp
/// @brief ParameterUIHints specializations for post-encoder module parameter structs.

#ifndef WHISKERTOOLBOX_POST_ENCODER_PARAM_SCHEMAS_HPP
#define WHISKERTOOLBOX_POST_ENCODER_PARAM_SCHEMAS_HPP

#include "ParameterSchema/ParameterSchema.hpp"
#include "PostEncoderModuleFactory.hpp"

namespace WhiskerToolbox::Transforms::V2 {

// GlobalAvgPoolModuleParams has no user-configurable fields — no hints needed.

template<>
struct ParameterUIHints<dl::SpatialPointModuleParams> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("interpolation")) {
            f->tooltip = "Interpolation strategy for spatial feature extraction";
        }
    }
};

}// namespace WhiskerToolbox::Transforms::V2

#endif// WHISKERTOOLBOX_POST_ENCODER_PARAM_SCHEMAS_HPP
