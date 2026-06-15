/// @file BindingTypes.test.cpp
/// @brief Tests for DeepLearning slot binding types and encoder/decoder variants.

#include <catch2/catch_test_macros.hpp>

#include "DeepLearning/bindings/BindingParamSchemas.hpp"
#include "DeepLearning/bindings/EncoderDecoderBindingTypes.hpp"
#include "DeepLearning/bindings/SlotBindingTypes.hpp"

#include "ParameterSchema/ParameterSchema.hpp"

#include <rfl/json.hpp>

// ============================================================================
// PostEncoderStepDescriptor
// ============================================================================

TEST_CASE("PostEncoderStepDescriptor schema extraction",
          "[dl_bindings][param_schema][post_encoder]") {
    auto schema = extractParameterSchema<dl::PostEncoderStepDescriptor>();
    CHECK(schema.field("module_key") != nullptr);
    CHECK(schema.field("parameters_json") != nullptr);
}

TEST_CASE("PostEncoderStepDescriptor JSON round-trip",
          "[dl_bindings][param_schema][post_encoder][roundtrip]") {
    dl::PostEncoderStepDescriptor desc;
    desc.module_key = "spatial_point";
    desc.parameters_json = R"({"interpolation":"Bilinear","point_key":"pts/a"})";

    auto json = rfl::json::write(desc);
    auto result = rfl::json::read<dl::PostEncoderStepDescriptor>(json);
    REQUIRE(result);
    CHECK(result.value().module_key == "spatial_point");
    CHECK(result.value().parameters_json == desc.parameters_json);
}

// ============================================================================
// DecoderVariant
// ============================================================================

TEST_CASE("DecoderVariant schema extraction",
          "[dl_bindings][param_schema][decoder]") {
    struct Wrapper {
        dl::DecoderVariant decoder = dl::MaskDecoderParams{};
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
}

TEST_CASE("DecoderVariant JSON round-trip",
          "[dl_bindings][param_schema][decoder][roundtrip]") {
    SECTION("MaskDecoder") {
        dl::DecoderVariant var{dl::MaskDecoderParams{.threshold = 0.7f}};
        auto json = rfl::json::write(var);
        auto result = rfl::json::read<dl::DecoderVariant>(json);
        REQUIRE(result);
    }

    SECTION("PointDecoder") {
        dl::DecoderVariant var{
                dl::PointDecoderParams{.subpixel = false, .threshold = 0.3f}};
        auto json = rfl::json::write(var);
        auto result = rfl::json::read<dl::DecoderVariant>(json);
        REQUIRE(result);
    }

    SECTION("LineDecoder") {
        dl::DecoderVariant var{dl::LineDecoderParams{.threshold = 0.6f}};
        auto json = rfl::json::write(var);
        auto result = rfl::json::read<dl::DecoderVariant>(json);
        REQUIRE(result);
    }

    SECTION("FeatureVectorDecoder") {
        dl::DecoderVariant var{dl::FeatureVectorDecoderParams{}};
        auto json = rfl::json::write(var);
        auto result = rfl::json::read<dl::DecoderVariant>(json);
        REQUIRE(result);
    }
}

// ============================================================================
// EncoderVariant
// ============================================================================

TEST_CASE("EncoderVariant schema extraction",
          "[dl_bindings][param_schema][encoder_variant]") {
    struct Wrapper {
        dl::EncoderVariant encoder = dl::ImageEncoderParams{};
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
}

TEST_CASE("EncoderVariant JSON round-trip",
          "[dl_bindings][param_schema][encoder_variant][roundtrip]") {
    SECTION("ImageEncoder") {
        dl::EncoderVariant var{dl::ImageEncoderParams{.normalize = false}};
        auto json = rfl::json::write(var);
        auto result = rfl::json::read<dl::EncoderVariant>(json);
        REQUIRE(result);
    }

    SECTION("Point2DEncoder with Heatmap mode") {
        dl::Point2DEncoderParams params{
                .mode = dl::RasterMode::Heatmap,
                .gaussian_sigma = 5.0f,
                .normalize = true};
        dl::EncoderVariant var{params};
        auto json = rfl::json::write(var);
        auto result = rfl::json::read<dl::EncoderVariant>(json);
        REQUIRE(result);
    }

    SECTION("Mask2DEncoder") {
        dl::EncoderVariant var{dl::Mask2DEncoderParams{}};
        auto json = rfl::json::write(var);
        auto result = rfl::json::read<dl::EncoderVariant>(json);
        REQUIRE(result);
    }

    SECTION("Line2DEncoder") {
        dl::EncoderVariant var{dl::Line2DEncoderParams{}};
        auto json = rfl::json::write(var);
        auto result = rfl::json::read<dl::EncoderVariant>(json);
        REQUIRE(result);
    }
}

// ============================================================================
// SlotBindingData / OutputBindingData
// ============================================================================

TEST_CASE("SlotBindingData JSON round-trip",
          "[dl_bindings][slot_binding][roundtrip]") {
    SlotBindingData binding{
            .slot_name = "encoder_image",
            .data_key = "media/video",
            .encoder = dl::ImageEncoderParams{.normalize = false},
            .time_offset = -1};

    auto json = rfl::json::write(binding);
    auto result = rfl::json::read<SlotBindingData>(json);
    REQUIRE(result);
    CHECK(result.value().slot_name == "encoder_image");
    CHECK(result.value().data_key == "media/video");
    CHECK(result.value().time_offset == -1);
}

TEST_CASE("OutputBindingData JSON round-trip",
          "[dl_bindings][slot_binding][roundtrip]") {
    OutputBindingData binding{
            .slot_name = "decoder_mask",
            .data_key = "masks/pred",
            .decoder = dl::MaskDecoderParams{.threshold = 0.5f}};

    auto json = rfl::json::write(binding);
    auto result = rfl::json::read<OutputBindingData>(json);
    REQUIRE(result);
    CHECK(result.value().slot_name == "decoder_mask");
    CHECK(result.value().data_key == "masks/pred");
}

// ============================================================================
// Binding form structs (AutoParamWidget)
// ============================================================================

TEST_CASE("DynamicInputBindingForm schema extraction",
          "[dl_bindings][param_schema][dynamic_input_form]") {
    auto schema = extractParameterSchema<dl::DynamicInputBindingForm>();
    REQUIRE(schema.fields.size() == 3);

    auto * data_key = schema.field("data_key");
    REQUIRE(data_key != nullptr);
    CHECK(data_key->dynamic_combo);
    CHECK(data_key->include_none_sentinel);
    CHECK(data_key->display_name == "Data Source");

    auto * encoder = schema.field("encoder");
    REQUIRE(encoder != nullptr);
    CHECK(encoder->is_variant);

    auto * time_offset = schema.field("time_offset");
    REQUIRE(time_offset != nullptr);
    CHECK(time_offset->display_name == "Time Offset");
}

TEST_CASE("OutputBindingForm schema extraction",
          "[dl_bindings][param_schema][output_form]") {
    auto schema = extractParameterSchema<dl::OutputBindingForm>();
    REQUIRE(schema.fields.size() == 2);

    auto * data_key = schema.field("data_key");
    REQUIRE(data_key != nullptr);
    CHECK(data_key->display_name == "Target");
    CHECK(data_key->dynamic_combo);

    auto * decoder = schema.field("decoder");
    REQUIRE(decoder != nullptr);
    CHECK(decoder->is_variant);
}

TEST_CASE("normalizeBindingDataKey strips None sentinel",
          "[dl_bindings][param_schema][form_helpers]") {
    CHECK(dl::normalizeBindingDataKey("(None)").empty());
    CHECK(dl::normalizeBindingDataKey("media/video") == "media/video");
}

TEST_CASE("toSlotBindingData maps form fields",
          "[dl_bindings][param_schema][form_helpers]") {
    dl::DynamicInputBindingForm form{
            .data_key = "(None)",
            .encoder = dl::ImageEncoderParams{.normalize = true},
            .time_offset = 2};
    auto binding = dl::toSlotBindingData("encoder_image", form);
    CHECK(binding.slot_name == "encoder_image");
    CHECK(binding.data_key.empty());
    CHECK(binding.time_offset == 2);
}

TEST_CASE("toOutputBindingData maps form fields",
          "[dl_bindings][param_schema][form_helpers]") {
    dl::OutputBindingForm form{
            .data_key = "masks/out",
            .decoder = dl::MaskDecoderParams{.threshold = 0.5f}};
    auto binding = dl::toOutputBindingData("mask_out", form);
    CHECK(binding.slot_name == "mask_out");
    CHECK(binding.data_key == "masks/out");
}
