/// @file DeepLearningParamSchemas.cpp
/// @brief ParameterUIHints implementations for widget-level deep learning
///        parameter structs.

#include "DeepLearningParamSchemas.hpp"


void ParameterUIHints<dl::widget::RelativeCaptureParams>::annotate(
        ParameterSchema & schema) {
    if (auto * f = schema.field("time_offset")) {
        f->tooltip = "Temporal offset from the current frame";
    }
}

void ParameterUIHints<dl::widget::StaticCaptureInitParams>::annotate(
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

void ParameterUIHints<dl::widget::StaticSequenceEntryParams>::annotate(
        ParameterSchema & schema) {
    if (auto * f = schema.field("data_key")) {
        f->tooltip = "DataManager key for this sequence entry";
        f->dynamic_combo = true;
        f->include_none_sentinel = true;
    }
    if (auto * f = schema.field("capture_mode_str")) {
        f->display_name = "Capture Mode";
        f->tooltip = "Relative: re-encode each run; Absolute: capture once";
        f->allowed_values = {"Relative", "Absolute"};
    }
    if (auto * f = schema.field("time_offset")) {
        f->tooltip = "Temporal offset from the current frame (Relative mode)";
    }
}

void ParameterUIHints<dl::widget::RecurrentSequenceEntryParams>::annotate(
        ParameterSchema & schema) {
    if (auto * f = schema.field("output_slot_name")) {
        f->display_name = "Output Slot";
        f->tooltip = "Which model output feeds back into this sequence entry";
        f->dynamic_combo = true;
        f->include_none_sentinel = true;
    }
    if (auto * f = schema.field("init_mode_str")) {
        f->display_name = "Init Mode";
        f->tooltip = "How to initialize recurrent state at t=0";
        f->allowed_values = {"Zeros", "StaticCapture", "FirstOutput"};
    }
}

void ParameterUIHints<dl::widget::SequenceEntryParams>::annotate(
        ParameterSchema & schema) {
    if (auto * f = schema.field("entry")) {
        f->tooltip = "Static data source or recurrent feedback for this position";
    }
}

void ParameterUIHints<dl::widget::StaticInputSlotParams>::annotate(
        ParameterSchema & schema) {
    if (auto * f = schema.field("source")) {
        f->display_name = "Data Source";
        f->tooltip = "DataManager key for the static input data";
        f->dynamic_combo = true;
        f->include_none_sentinel = true;
    }
    if (auto * f = schema.field("capture_mode")) {
        f->tooltip =
                "Relative: re-encode each run at current_frame + offset.\n"
                "Absolute: capture once at a chosen frame and reuse.";
    }
}

void ParameterUIHints<dl::widget::DynamicInputSlotParams>::annotate(
        ParameterSchema & schema) {
    if (auto * f = schema.field("source")) {
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

void ParameterUIHints<dl::widget::OutputSlotParams>::annotate(
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

void ParameterUIHints<dl::widget::EncoderShapeParams>::annotate(
        ParameterSchema & schema) {
    if (auto * f = schema.field("input_height")) {
        f->display_name = "Input Height";
        f->tooltip =
                "Input image height in pixels (resized to this before encoding)";
        f->max_value = 4096;
    }
    if (auto * f = schema.field("input_width")) {
        f->display_name = "Input Width";
        f->tooltip =
                "Input image width in pixels (resized to this before encoding)";
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

void ParameterUIHints<dl::widget::RecurrentBindingSlotParams>::annotate(
        ParameterSchema & schema) {
    if (auto * f = schema.field("output_slot_name")) {
        f->display_name = "Output Slot";
        f->tooltip =
                "Model output slot to feed back into this input at t+1";
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

void ParameterUIHints<dl::widget::PostEncoderSlotParams>::annotate(
        ParameterSchema & schema) {
    if (auto * f = schema.field("module")) {
        f->display_name = "Module";
        f->tooltip =
                "Optional post-processing applied to the encoder output tensor:\n"
                "• None: pass encoder output directly to the decoder\n"
                "• Global Average Pooling: [B,C,H,W] → [B,C] via adaptive avg pool\n"
                "• Spatial Point Extraction: extract features at a 2D point location";
    }
}
