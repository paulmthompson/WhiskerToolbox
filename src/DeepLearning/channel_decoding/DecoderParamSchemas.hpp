/// @file DecoderParamSchemas.hpp
/// @brief ParameterUIHints specializations for per-decoder parameter structs.

#ifndef WHISKERTOOLBOX_DECODER_PARAM_SCHEMAS_HPP
#define WHISKERTOOLBOX_DECODER_PARAM_SCHEMAS_HPP

#include "ChannelDecoder.hpp"
#include "ParameterSchema/ParameterSchema.hpp"

namespace WhiskerToolbox::Transforms::V2 {

template<>
struct ParameterUIHints<dl::MaskDecoderParams> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("threshold")) {
            f->tooltip = "Binary threshold for mask generation";
            f->min_value = 0.01;
            f->max_value = 1.0;
            f->is_exclusive_min = true;
        }
    }
};

template<>
struct ParameterUIHints<dl::PointDecoderParams> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("subpixel")) {
            f->tooltip = "Enable parabolic subpixel refinement for point localization";
        }
        if (auto * f = schema.field("threshold")) {
            f->tooltip = "Minimum activation for local maxima detection";
            f->min_value = 0.01;
            f->max_value = 1.0;
            f->is_exclusive_min = true;
        }
    }
};

template<>
struct ParameterUIHints<dl::LineDecoderParams> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("threshold")) {
            f->tooltip = "Binary threshold for line extraction";
            f->min_value = 0.01;
            f->max_value = 1.0;
            f->is_exclusive_min = true;
        }
    }
};

// FeatureVectorDecoderParams has no user-configurable fields — no hints needed.

}// namespace WhiskerToolbox::Transforms::V2

#endif// WHISKERTOOLBOX_DECODER_PARAM_SCHEMAS_HPP
