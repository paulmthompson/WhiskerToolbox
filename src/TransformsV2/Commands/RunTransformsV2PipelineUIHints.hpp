/**
 * @file RunTransformsV2PipelineUIHints.hpp
 * @brief ParameterUIHints for RunTransformsV2Pipeline command parameters
 */

#ifndef RUN_TRANSFORMS_V2_PIPELINE_UI_HINTS_HPP
#define RUN_TRANSFORMS_V2_PIPELINE_UI_HINTS_HPP

#include "Commands/RunTransformsV2Pipeline.hpp"

#include "ParameterSchema/ParameterSchema.hpp"

template<>
struct ParameterUIHints<commands::RunTransformsV2PipelineParams> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("input_key")) {
            f->dynamic_combo = true;
            f->tooltip = "DataManager key of the input data object";
        }
        if (auto * f = schema.field("output_key")) {
            f->dynamic_combo = true;
            f->tooltip = "DataManager key where pipeline output will be stored";
        }
        if (auto * f = schema.field("pipeline_path")) {
            f->path_field_kind = PathFieldKind::FilePath;
            f->file_dialog_id = "transformv2_pipeline_open";
            f->tooltip =
                    "Pipeline JSON file (Browse opens the user pipeline library directory)";
        }
        if (auto * f = schema.field("output_time_key")) {
            f->tooltip =
                    "TimeFrame key for output data; leave empty to inherit from input_key";
        }
    }
};

#endif// RUN_TRANSFORMS_V2_PIPELINE_UI_HINTS_HPP
