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
        if (auto * f = schema.field("point_key")) {
            f->display_name = "Point Key";
            f->tooltip =
                    "DataManager key for PointData supplying the per-frame query point\n"
                    "(only used when Spatial Point Extraction is selected)";
            f->dynamic_combo = true;
            f->include_none_sentinel = true;
        }
    }
};

}// namespace WhiskerToolbox::Transforms::V2

#endif// WHISKERTOOLBOX_POST_ENCODER_PARAM_SCHEMAS_HPP
