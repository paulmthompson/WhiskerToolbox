/// @file GeneralEncoderParamSchemas.hpp
/// @brief ParameterUIHints specializations for GeneralEncoderModelParams.

#ifndef NEURALYZER_GENERAL_ENCODER_PARAM_SCHEMAS_HPP
#define NEURALYZER_GENERAL_ENCODER_PARAM_SCHEMAS_HPP

#include "GeneralEncoderModelParams.hpp"

#include "ParameterSchema/ParameterSchema.hpp"


template<>
struct ParameterUIHints<dl::GeneralEncoderModelParams> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("input_height")) {
            f->display_name = "Input Height";
            f->tooltip =
                    "Input image height in pixels (resized to this before encoding)";
            f->min_value = 1;
            f->max_value = 4096;
        }
        if (auto * f = schema.field("input_width")) {
            f->display_name = "Input Width";
            f->tooltip =
                    "Input image width in pixels (resized to this before encoding)";
            f->min_value = 1;
            f->max_value = 4096;
        }
        if (auto * f = schema.field("output_shape")) {
            f->display_name = "Output Shape";
            f->tooltip =
                    "Comma-separated output dimensions (excluding batch), e.g.:\n"
                    "  384,7,7   — spatial feature map\n"
                    "  768,16,16 — larger backbone\n"
                    "  512       — 1D feature vector";
        }
    }
};


#endif// NEURALYZER_GENERAL_ENCODER_PARAM_SCHEMAS_HPP
