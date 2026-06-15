/// @file BindingParamSchemas.cpp
/// @brief ParameterUIHints implementations for slot binding form structs.

#include "bindings/BindingParamSchemas.hpp"

void ParameterUIHints<dl::DynamicInputBindingForm>::annotate(
        ParameterSchema & schema) {
    if (auto * f = schema.field("data_key")) {
        f->display_name = "Data Source";
        f->tooltip = "DataManager key for the input data";
        f->dynamic_combo = true;
        f->include_none_sentinel = true;
    }
    if (auto * f = schema.field("encoder")) {
        f->tooltip = "Encoder type and configuration for this input";
    }
    if (auto * f = schema.field("time_offset")) {
        f->display_name = "Time Offset";
        f->tooltip =
                "Temporal offset applied to each frame during encoding.\n"
                "E.g. -1 reads one frame behind, +1 reads one frame ahead.";
    }
}

void ParameterUIHints<dl::OutputBindingForm>::annotate(
        ParameterSchema & schema) {
    if (auto * f = schema.field("data_key")) {
        f->display_name = "Target";
        f->tooltip = "DataManager key where decoded results are written";
        f->dynamic_combo = true;
        f->include_none_sentinel = true;
    }
    if (auto * f = schema.field("decoder")) {
        f->tooltip = "Decoder type and configuration for this output";
    }
}
