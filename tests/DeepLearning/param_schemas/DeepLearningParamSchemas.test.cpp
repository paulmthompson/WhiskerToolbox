/// @file DeepLearningParamSchemas.test.cpp
/// @brief Tests for widget-level parameter schemas and tagged unions.
///
/// Verifies schema extraction, UIHints annotation, and JSON round-trips
/// for all dl::widget variant types and composite structs.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "DeepLearning_Widget/Core/DeepLearningParamSchemas.hpp"

#include <rfl/json.hpp>

using WhiskerToolbox::Transforms::V2::extractParameterSchema;

// ============================================================================
// CaptureModeVariant
// ============================================================================

TEST_CASE("CaptureModeVariant schema extraction",
          "[dl_widget][param_schema][capture_mode]") {
    struct Wrapper {
        dl::widget::CaptureModeVariant capture = dl::widget::RelativeCaptureParams{};
    };

    auto schema = extractParameterSchema<Wrapper>();
    REQUIRE(schema.fields.size() == 1);

    auto * f = schema.field("capture");
    REQUIRE(f != nullptr);
    CHECK(f->is_variant);
    CHECK(f->type_name == "variant");
    CHECK(f->variant_discriminator == "capture_mode");
    REQUIRE(f->variant_alternatives.size() == 2);
    CHECK(f->allowed_values.size() == 2);

    SECTION("RelativeCaptureParams alternative") {
        auto const & alt = f->variant_alternatives[0];
        CHECK(alt.tag == "RelativeCaptureParams");
        REQUIRE(alt.schema != nullptr);
        CHECK(alt.schema->fields.size() == 1);
        CHECK(alt.schema->field("time_offset") != nullptr);
    }

    SECTION("AbsoluteCaptureParams alternative") {
        auto const & alt = f->variant_alternatives[1];
        CHECK(alt.tag == "AbsoluteCaptureParams");
        REQUIRE(alt.schema != nullptr);
        CHECK(alt.schema->fields.size() == 1);
        CHECK(alt.schema->field("captured_frame") != nullptr);
    }
}

TEST_CASE("CaptureModeVariant JSON round-trip",
          "[dl_widget][param_schema][capture_mode][roundtrip]") {
    SECTION("Relative capture") {
        dl::widget::RelativeCaptureParams params{.time_offset = -5};
        dl::widget::CaptureModeVariant var{params};
        auto json = rfl::json::write(var);
        auto result = rfl::json::read<dl::widget::CaptureModeVariant>(json);
        REQUIRE(result);
    }

    SECTION("Absolute capture") {
        dl::widget::AbsoluteCaptureParams params{.captured_frame = 42};
        dl::widget::CaptureModeVariant var{params};
        auto json = rfl::json::write(var);
        auto result = rfl::json::read<dl::widget::CaptureModeVariant>(json);
        REQUIRE(result);
    }
}

// ============================================================================
// RecurrentInitVariant
// ============================================================================

TEST_CASE("RecurrentInitVariant schema extraction",
          "[dl_widget][param_schema][recurrent_init]") {
    struct Wrapper {
        dl::widget::RecurrentInitVariant init = dl::widget::ZerosInitParams{};
    };

    auto schema = extractParameterSchema<Wrapper>();
    REQUIRE(schema.fields.size() == 1);

    auto * f = schema.field("init");
    REQUIRE(f != nullptr);
    CHECK(f->is_variant);
    CHECK(f->variant_discriminator == "init_mode");
    REQUIRE(f->variant_alternatives.size() == 3);

    CHECK(f->variant_alternatives[0].tag == "ZerosInitParams");
    CHECK(f->variant_alternatives[1].tag == "StaticCaptureInitParams");
    CHECK(f->variant_alternatives[2].tag == "FirstOutputInitParams");

    SECTION("ZerosInitParams has no fields") {
        CHECK(f->variant_alternatives[0].schema->fields.empty());
    }

    SECTION("StaticCaptureInitParams has data_key and frame") {
        auto const & s = *f->variant_alternatives[1].schema;
        CHECK(s.fields.size() == 2);
        CHECK(s.field("data_key") != nullptr);
        CHECK(s.field("frame") != nullptr);
    }

    SECTION("FirstOutputInitParams has no fields") {
        CHECK(f->variant_alternatives[2].schema->fields.empty());
    }
}

TEST_CASE("RecurrentInitVariant JSON round-trip",
          "[dl_widget][param_schema][recurrent_init][roundtrip]") {
    SECTION("Zeros init") {
        dl::widget::RecurrentInitVariant var{dl::widget::ZerosInitParams{}};
        auto json = rfl::json::write(var);
        auto result = rfl::json::read<dl::widget::RecurrentInitVariant>(json);
        REQUIRE(result);
    }

    SECTION("StaticCapture init") {
        dl::widget::StaticCaptureInitParams params{
                .data_key = "media/video_1",
                .frame = 10};
        dl::widget::RecurrentInitVariant var{params};
        auto json = rfl::json::write(var);
        auto result = rfl::json::read<dl::widget::RecurrentInitVariant>(json);
        REQUIRE(result);
    }

    SECTION("FirstOutput init") {
        dl::widget::RecurrentInitVariant var{dl::widget::FirstOutputInitParams{}};
        auto json = rfl::json::write(var);
        auto result = rfl::json::read<dl::widget::RecurrentInitVariant>(json);
        REQUIRE(result);
    }
}

// ============================================================================
// SequenceEntryVariant
// ============================================================================

TEST_CASE("SequenceEntryVariant schema extraction",
          "[dl_widget][param_schema][sequence_entry]") {
    struct Wrapper {
        dl::widget::SequenceEntryVariant entry =
                dl::widget::StaticSequenceEntryParams{};
    };

    auto schema = extractParameterSchema<Wrapper>();
    auto * f = schema.field("entry");
    REQUIRE(f != nullptr);
    CHECK(f->is_variant);
    CHECK(f->variant_discriminator == "source_type");
    REQUIRE(f->variant_alternatives.size() == 2);

    CHECK(f->variant_alternatives[0].tag == "StaticSequenceEntryParams");
    CHECK(f->variant_alternatives[1].tag == "RecurrentSequenceEntryParams");

    SECTION("StaticSequenceEntryParams fields") {
        auto const & s = *f->variant_alternatives[0].schema;
        CHECK(s.field("data_key") != nullptr);
        CHECK(s.field("capture_mode_str") != nullptr);
        CHECK(s.field("time_offset") != nullptr);
    }

    SECTION("RecurrentSequenceEntryParams fields") {
        auto const & s = *f->variant_alternatives[1].schema;
        CHECK(s.field("output_slot_name") != nullptr);
        CHECK(s.field("init_mode_str") != nullptr);
    }
}

TEST_CASE("SequenceEntryVariant JSON round-trip",
          "[dl_widget][param_schema][sequence_entry][roundtrip]") {
    SECTION("Static entry") {
        dl::widget::StaticSequenceEntryParams params{
                .data_key = "points/whisker",
                .capture_mode_str = "Absolute",
                .time_offset = 3};
        dl::widget::SequenceEntryVariant var{params};
        auto json = rfl::json::write(var);
        auto result = rfl::json::read<dl::widget::SequenceEntryVariant>(json);
        REQUIRE(result);
    }

    SECTION("Recurrent entry") {
        dl::widget::RecurrentSequenceEntryParams params{
                .output_slot_name = "decoder_mask",
                .init_mode_str = "Zeros"};
        dl::widget::SequenceEntryVariant var{params};
        auto json = rfl::json::write(var);
        auto result = rfl::json::read<dl::widget::SequenceEntryVariant>(json);
        REQUIRE(result);
    }
}

// ============================================================================
// PostEncoderVariant
// ============================================================================

TEST_CASE("PostEncoderVariant schema extraction",
          "[dl_widget][param_schema][post_encoder]") {
    struct Wrapper {
        dl::widget::PostEncoderVariant module = dl::widget::NoPostEncoderParams{};
    };

    auto schema = extractParameterSchema<Wrapper>();
    auto * f = schema.field("module");
    REQUIRE(f != nullptr);
    CHECK(f->is_variant);
    CHECK(f->variant_discriminator == "module");
    REQUIRE(f->variant_alternatives.size() == 3);

    CHECK(f->variant_alternatives[0].tag == "NoPostEncoderParams");
    CHECK(f->variant_alternatives[1].tag == "GlobalAvgPoolModuleParams");
    CHECK(f->variant_alternatives[2].tag == "SpatialPointModuleParams");

    SECTION("NoPostEncoderParams has no fields") {
        CHECK(f->variant_alternatives[0].schema->fields.empty());
    }

    SECTION("GlobalAvgPoolModuleParams has no fields") {
        CHECK(f->variant_alternatives[1].schema->fields.empty());
    }

    SECTION("SpatialPointModuleParams has interpolation field") {
        auto const & s = *f->variant_alternatives[2].schema;
        CHECK(s.fields.size() == 1);
        auto * interp = s.field("interpolation");
        REQUIRE(interp != nullptr);
        CHECK(interp->type_name == "enum");
    }
}

TEST_CASE("PostEncoderVariant JSON round-trip",
          "[dl_widget][param_schema][post_encoder][roundtrip]") {
    SECTION("No post-encoder") {
        dl::widget::PostEncoderVariant var{dl::widget::NoPostEncoderParams{}};
        auto json = rfl::json::write(var);
        auto result = rfl::json::read<dl::widget::PostEncoderVariant>(json);
        REQUIRE(result);
    }

    SECTION("GlobalAvgPool") {
        dl::widget::PostEncoderVariant var{dl::GlobalAvgPoolModuleParams{}};
        auto json = rfl::json::write(var);
        auto result = rfl::json::read<dl::widget::PostEncoderVariant>(json);
        REQUIRE(result);
    }

    SECTION("SpatialPoint with Bilinear") {
        dl::SpatialPointModuleParams params{
                .interpolation = dl::InterpolationMode::Bilinear};
        dl::widget::PostEncoderVariant var{params};
        auto json = rfl::json::write(var);
        auto result = rfl::json::read<dl::widget::PostEncoderVariant>(json);
        REQUIRE(result);
    }
}

// ============================================================================
// DecoderVariant
// ============================================================================

TEST_CASE("DecoderVariant schema extraction",
          "[dl_widget][param_schema][decoder]") {
    struct Wrapper {
        dl::widget::DecoderVariant decoder = dl::MaskDecoderParams{};
    };

    auto schema = extractParameterSchema<Wrapper>();
    auto * f = schema.field("decoder");
    REQUIRE(f != nullptr);
    CHECK(f->is_variant);
    CHECK(f->variant_discriminator == "decoder");
    REQUIRE(f->variant_alternatives.size() == 4);

    CHECK(f->variant_alternatives[0].tag == "MaskDecoderParams");
    CHECK(f->variant_alternatives[1].tag == "PointDecoderParams");
    CHECK(f->variant_alternatives[2].tag == "LineDecoderParams");
    CHECK(f->variant_alternatives[3].tag == "FeatureVectorDecoderParams");

    SECTION("MaskDecoderParams has threshold") {
        auto const & s = *f->variant_alternatives[0].schema;
        CHECK(s.fields.size() == 1);
        CHECK(s.field("threshold") != nullptr);
    }

    SECTION("PointDecoderParams has subpixel and threshold") {
        auto const & s = *f->variant_alternatives[1].schema;
        CHECK(s.fields.size() == 2);
        CHECK(s.field("subpixel") != nullptr);
        CHECK(s.field("threshold") != nullptr);
    }

    SECTION("LineDecoderParams has threshold") {
        auto const & s = *f->variant_alternatives[2].schema;
        CHECK(s.fields.size() == 1);
        CHECK(s.field("threshold") != nullptr);
    }

    SECTION("FeatureVectorDecoderParams has no fields") {
        CHECK(f->variant_alternatives[3].schema->fields.empty());
    }
}

TEST_CASE("DecoderVariant JSON round-trip",
          "[dl_widget][param_schema][decoder][roundtrip]") {
    SECTION("MaskDecoder") {
        dl::widget::DecoderVariant var{dl::MaskDecoderParams{.threshold = 0.7f}};
        auto json = rfl::json::write(var);
        auto result = rfl::json::read<dl::widget::DecoderVariant>(json);
        REQUIRE(result);
    }

    SECTION("PointDecoder") {
        dl::widget::DecoderVariant var{
                dl::PointDecoderParams{.subpixel = false, .threshold = 0.3f}};
        auto json = rfl::json::write(var);
        auto result = rfl::json::read<dl::widget::DecoderVariant>(json);
        REQUIRE(result);
    }

    SECTION("LineDecoder") {
        dl::widget::DecoderVariant var{dl::LineDecoderParams{.threshold = 0.6f}};
        auto json = rfl::json::write(var);
        auto result = rfl::json::read<dl::widget::DecoderVariant>(json);
        REQUIRE(result);
    }

    SECTION("FeatureVectorDecoder") {
        dl::widget::DecoderVariant var{dl::FeatureVectorDecoderParams{}};
        auto json = rfl::json::write(var);
        auto result = rfl::json::read<dl::widget::DecoderVariant>(json);
        REQUIRE(result);
    }
}

// ============================================================================
// OutputSlotParams (composite struct with variant field)
// ============================================================================

TEST_CASE("OutputSlotParams schema extraction",
          "[dl_widget][param_schema][output_slot]") {
    auto schema = extractParameterSchema<dl::widget::OutputSlotParams>();

    CHECK(schema.fields.size() == 2);

    SECTION("data_key field") {
        auto * f = schema.field("data_key");
        REQUIRE(f != nullptr);
        CHECK(f->type_name == "std::string");
        CHECK(f->tooltip == "DataManager key where decoded results are written");
    }

    SECTION("decoder variant field") {
        auto * f = schema.field("decoder");
        REQUIRE(f != nullptr);
        CHECK(f->is_variant);
        CHECK(f->variant_discriminator == "decoder");
        CHECK(f->variant_alternatives.size() == 4);
        CHECK(f->tooltip == "Decoder type and configuration for this output");
    }
}

TEST_CASE("OutputSlotParams JSON round-trip",
          "[dl_widget][param_schema][output_slot][roundtrip]") {
    SECTION("Default (MaskDecoder)") {
        dl::widget::OutputSlotParams params;
        params.data_key = "output/masks";
        auto json = rfl::json::write(params);
        auto result = rfl::json::read<dl::widget::OutputSlotParams>(json);
        REQUIRE(result);
        CHECK(result.value().data_key == "output/masks");
    }

    SECTION("With PointDecoder") {
        dl::widget::OutputSlotParams params;
        params.data_key = "output/points";
        params.decoder = dl::PointDecoderParams{.subpixel = true, .threshold = 0.4f};
        auto json = rfl::json::write(params);
        auto result = rfl::json::read<dl::widget::OutputSlotParams>(json);
        REQUIRE(result);
        CHECK(result.value().data_key == "output/points");
    }
}

// ============================================================================
// EncoderShapeParams
// ============================================================================

TEST_CASE("EncoderShapeParams schema extraction",
          "[dl_widget][param_schema][encoder_shape]") {
    auto schema = extractParameterSchema<dl::widget::EncoderShapeParams>();

    CHECK(schema.fields.size() == 3);

    SECTION("input_height field") {
        auto * f = schema.field("input_height");
        REQUIRE(f != nullptr);
        CHECK(f->tooltip == "Input image height in pixels (resized to this before encoding)");
    }

    SECTION("input_width field") {
        auto * f = schema.field("input_width");
        REQUIRE(f != nullptr);
        CHECK(f->tooltip == "Input image width in pixels (resized to this before encoding)");
    }

    SECTION("output_shape field") {
        auto * f = schema.field("output_shape");
        REQUIRE(f != nullptr);
        CHECK(f->type_name == "std::string");
        CHECK(f->tooltip ==
              "Comma-separated output dimensions (excluding batch), e.g.:\n"
              "  384,7,7   — spatial feature map\n"
              "  768,16,16 — larger backbone\n"
              "  512       — 1D feature vector");
    }
}

TEST_CASE("EncoderShapeParams JSON round-trip",
          "[dl_widget][param_schema][encoder_shape][roundtrip]") {
    dl::widget::EncoderShapeParams params;
    params.input_height = 320;
    params.input_width = 256;
    params.output_shape = "768,16,16";

    auto json = rfl::json::write(params);
    auto result = rfl::json::read<dl::widget::EncoderShapeParams>(json);
    REQUIRE(result);
    CHECK(result.value().input_height.value() == 320);
    CHECK(result.value().input_width.value() == 256);
    CHECK(result.value().output_shape == "768,16,16");
}

// ============================================================================
// UIHints annotation tests
// ============================================================================

TEST_CASE("StaticSequenceEntryParams UIHints annotate allowed_values",
          "[dl_widget][param_schema][uihints]") {
    auto schema = extractParameterSchema<dl::widget::StaticSequenceEntryParams>();

    auto * f = schema.field("capture_mode_str");
    REQUIRE(f != nullptr);
    CHECK(f->display_name == "Capture Mode");
    REQUIRE(f->allowed_values.size() == 2);
    CHECK(f->allowed_values[0] == "Relative");
    CHECK(f->allowed_values[1] == "Absolute");
}

TEST_CASE("RecurrentSequenceEntryParams UIHints annotate allowed_values",
          "[dl_widget][param_schema][uihints]") {
    auto schema = extractParameterSchema<dl::widget::RecurrentSequenceEntryParams>();

    auto * f = schema.field("init_mode_str");
    REQUIRE(f != nullptr);
    CHECK(f->display_name == "Init Mode");
    REQUIRE(f->allowed_values.size() == 3);
    CHECK(f->allowed_values[0] == "Zeros");
    CHECK(f->allowed_values[1] == "StaticCapture");
    CHECK(f->allowed_values[2] == "FirstOutput");
}

// ============================================================================
// EncoderVariant
// ============================================================================

TEST_CASE("EncoderVariant schema extraction",
          "[dl_widget][param_schema][encoder_variant]") {
    struct Wrapper {
        dl::widget::EncoderVariant encoder = dl::ImageEncoderParams{};
    };

    auto schema = extractParameterSchema<Wrapper>();
    auto * f = schema.field("encoder");
    REQUIRE(f != nullptr);
    CHECK(f->is_variant);
    CHECK(f->variant_discriminator == "encoder");
    REQUIRE(f->variant_alternatives.size() == 4);

    CHECK(f->variant_alternatives[0].tag == "ImageEncoderParams");
    CHECK(f->variant_alternatives[1].tag == "Point2DEncoderParams");
    CHECK(f->variant_alternatives[2].tag == "Mask2DEncoderParams");
    CHECK(f->variant_alternatives[3].tag == "Line2DEncoderParams");

    SECTION("ImageEncoderParams has normalize field") {
        auto const & s = *f->variant_alternatives[0].schema;
        CHECK(s.fields.size() == 1);
        CHECK(s.field("normalize") != nullptr);
    }

    SECTION("Point2DEncoderParams has mode, gaussian_sigma, normalize") {
        auto const & s = *f->variant_alternatives[1].schema;
        CHECK(s.fields.size() == 3);
        CHECK(s.field("mode") != nullptr);
        CHECK(s.field("gaussian_sigma") != nullptr);
        CHECK(s.field("normalize") != nullptr);
    }

    SECTION("Mask2DEncoderParams has mode and normalize") {
        auto const & s = *f->variant_alternatives[2].schema;
        CHECK(s.fields.size() == 2);
        CHECK(s.field("mode") != nullptr);
        CHECK(s.field("normalize") != nullptr);
    }

    SECTION("Line2DEncoderParams has mode, gaussian_sigma, normalize") {
        auto const & s = *f->variant_alternatives[3].schema;
        CHECK(s.fields.size() == 3);
        CHECK(s.field("mode") != nullptr);
        CHECK(s.field("gaussian_sigma") != nullptr);
        CHECK(s.field("normalize") != nullptr);
    }
}

TEST_CASE("EncoderVariant JSON round-trip",
          "[dl_widget][param_schema][encoder_variant][roundtrip]") {
    SECTION("ImageEncoder") {
        dl::widget::EncoderVariant var{dl::ImageEncoderParams{.normalize = false}};
        auto json = rfl::json::write(var);
        auto result = rfl::json::read<dl::widget::EncoderVariant>(json);
        REQUIRE(result);
    }

    SECTION("Point2DEncoder with Heatmap mode") {
        dl::Point2DEncoderParams params{
                .mode = dl::RasterMode::Heatmap,
                .gaussian_sigma = 5.0f,
                .normalize = true};
        dl::widget::EncoderVariant var{params};
        auto json = rfl::json::write(var);
        auto result = rfl::json::read<dl::widget::EncoderVariant>(json);
        REQUIRE(result);
    }

    SECTION("Mask2DEncoder") {
        dl::widget::EncoderVariant var{dl::Mask2DEncoderParams{}};
        auto json = rfl::json::write(var);
        auto result = rfl::json::read<dl::widget::EncoderVariant>(json);
        REQUIRE(result);
    }

    SECTION("Line2DEncoder") {
        dl::widget::EncoderVariant var{dl::Line2DEncoderParams{}};
        auto json = rfl::json::write(var);
        auto result = rfl::json::read<dl::widget::EncoderVariant>(json);
        REQUIRE(result);
    }
}

// ============================================================================
// DynamicInputSlotParams
// ============================================================================

TEST_CASE("DynamicInputSlotParams schema extraction",
          "[dl_widget][param_schema][dynamic_input]") {
    auto schema = extractParameterSchema<dl::widget::DynamicInputSlotParams>();
    REQUIRE(schema.fields.size() == 3);

    SECTION("source field is a dynamic combo string") {
        auto * f = schema.field("source");
        REQUIRE(f != nullptr);
        CHECK(f->type_name == "std::string");
        CHECK(f->dynamic_combo);
        CHECK(f->include_none_sentinel);
        CHECK(f->display_name == "Data Source");
    }

    SECTION("encoder field is a variant") {
        auto * f = schema.field("encoder");
        REQUIRE(f != nullptr);
        CHECK(f->is_variant);
        CHECK(f->variant_discriminator == "encoder");
        CHECK(f->variant_alternatives.size() == 4);
    }

    SECTION("time_offset is an int field") {
        auto * f = schema.field("time_offset");
        REQUIRE(f != nullptr);
        CHECK(f->type_name == "int");
    }
}

TEST_CASE("DynamicInputSlotParams JSON round-trip",
          "[dl_widget][param_schema][dynamic_input][roundtrip]") {
    SECTION("Default params") {
        dl::widget::DynamicInputSlotParams params;
        auto json = rfl::json::write(params);
        auto result = rfl::json::read<dl::widget::DynamicInputSlotParams>(json);
        REQUIRE(result);
        CHECK(result.value().source.empty());
        CHECK(result.value().time_offset == 0);
    }

    SECTION("Fully specified params") {
        dl::widget::DynamicInputSlotParams params{
                .source = "media/video_1",
                .encoder = dl::Point2DEncoderParams{
                        .mode = dl::RasterMode::Heatmap,
                        .gaussian_sigma = 3.5f,
                        .normalize = false},
                .time_offset = -2};
        auto json = rfl::json::write(params);
        auto result = rfl::json::read<dl::widget::DynamicInputSlotParams>(json);
        REQUIRE(result);
        CHECK(result.value().source == "media/video_1");
        CHECK(result.value().time_offset == -2);
    }
}

TEST_CASE("DynamicInputSlotParams UIHints annotation",
          "[dl_widget][param_schema][dynamic_input][uihints]") {
    auto schema = extractParameterSchema<dl::widget::DynamicInputSlotParams>();

    auto * source = schema.field("source");
    REQUIRE(source != nullptr);
    CHECK(source->display_name == "Data Source");
    CHECK(source->dynamic_combo);
    CHECK(source->include_none_sentinel);

    auto * time_off = schema.field("time_offset");
    REQUIRE(time_off != nullptr);
    CHECK(time_off->display_name == "Time Offset");
}
