/// @file EncoderParamSchemas.hpp
/// @brief ParameterUIHints specializations for per-encoder parameter structs.

#ifndef WHISKERTOOLBOX_ENCODER_PARAM_SCHEMAS_HPP
#define WHISKERTOOLBOX_ENCODER_PARAM_SCHEMAS_HPP

#include "ChannelEncoder.hpp"
#include "ParameterSchema/ParameterSchema.hpp"

namespace WhiskerToolbox::Transforms::V2 {

template<>
struct ParameterUIHints<dl::ImageEncoderParams> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("normalize")) {
            f->tooltip = "Normalize pixel values to [0, 1]";
        }
    }
};

template<>
struct ParameterUIHints<dl::Point2DEncoderParams> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("mode")) {
            f->tooltip = "How to rasterize points onto the tensor channel";
            f->allowed_values = {"Binary", "Heatmap"};
        }
        if (auto * f = schema.field("gaussian_sigma")) {
            f->tooltip = "Sigma for Gaussian heatmap rendering (Heatmap mode only)";
            f->min_value = 0.0;
            f->max_value = 50.0;
            f->is_exclusive_min = true;
        }
        if (auto * f = schema.field("normalize")) {
            f->tooltip = "Normalize output channel to [0, 1]";
        }
    }
};

template<>
struct ParameterUIHints<dl::Mask2DEncoderParams> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("mode")) {
            f->tooltip = "Rasterization mode for masks";
            f->allowed_values = {"Binary"};
        }
        if (auto * f = schema.field("normalize")) {
            f->tooltip = "Normalize output channel to [0, 1]";
        }
    }
};

template<>
struct ParameterUIHints<dl::Line2DEncoderParams> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("mode")) {
            f->tooltip = "How to rasterize lines onto the tensor channel";
            f->allowed_values = {"Binary", "Heatmap"};
        }
        if (auto * f = schema.field("gaussian_sigma")) {
            f->tooltip = "Sigma for Gaussian heatmap rendering (Heatmap mode only)";
            f->min_value = 0.0;
            f->max_value = 50.0;
            f->is_exclusive_min = true;
        }
        if (auto * f = schema.field("normalize")) {
            f->tooltip = "Normalize output channel to [0, 1]";
        }
    }
};

}// namespace WhiskerToolbox::Transforms::V2

#endif// WHISKERTOOLBOX_ENCODER_PARAM_SCHEMAS_HPP
