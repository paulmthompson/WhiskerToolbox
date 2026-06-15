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

void ParameterUIHints<dl::DataManagerStaticSource>::annotate(
        ParameterSchema & schema) {
    if (auto * f = schema.field("data_key")) {
        f->display_name = "Data Source";
        f->tooltip = "DataManager key to re-encode each run";
        f->dynamic_combo = true;
        f->include_none_sentinel = true;
    }
    if (auto * f = schema.field("time_offset")) {
        f->tooltip = "Temporal offset from the current frame";
    }
}

void ParameterUIHints<dl::DataBankStaticSource>::annotate(
        ParameterSchema & schema) {
    if (auto * f = schema.field("bank_entry_id")) {
        f->display_name = "Bank Entry";
        f->tooltip = "Named DataBank entry with a pre-encoded tensor";
        f->dynamic_combo = true;
        f->include_none_sentinel = true;
    }
}

void ParameterUIHints<dl::StaticFrameSourceForm>::annotate(
        ParameterSchema & schema) {
    if (auto * f = schema.field("source")) {
        f->display_name = "Source";
        f->tooltip =
                "DataManager-relative encoding or a pre-captured DataBank entry";
    }
}

void ParameterUIHints<dl::StaticCaptureInit>::annotate(
        ParameterSchema & schema) {
    if (auto * f = schema.field("data_key")) {
        f->tooltip = "DataManager key for the static data source to encode";
        f->dynamic_combo = true;
        f->include_none_sentinel = true;
    }
    if (auto * f = schema.field("frame")) {
        f->tooltip = "Frame index to capture for initialization";
    }
}

void ParameterUIHints<dl::RecurrentFrameSourceForm>::annotate(
        ParameterSchema & schema) {
    if (auto * f = schema.field("output_slot_name")) {
        f->display_name = "Output Slot";
        f->tooltip =
                "Model output slot to feed back into this memory position at t+1";
        f->dynamic_combo = true;
        f->include_none_sentinel = true;
    }
    if (auto * f = schema.field("init")) {
        f->display_name = "Init Mode";
        f->tooltip =
                "How to initialize this input at t=0:\n"
                "• Zeros: all-zeros tensor\n"
                "• Static Capture: use a data source at a specific frame\n"
                "• First Output: run model once with zeros, use output as init";
    }
}
