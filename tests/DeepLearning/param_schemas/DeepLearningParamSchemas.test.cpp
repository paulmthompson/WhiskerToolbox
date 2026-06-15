/// @file DeepLearningParamSchemas.test.cpp
/// @brief Tests for widget-level parameter schemas and tagged unions.
///
/// Verifies schema extraction, UIHints annotation, and JSON round-trips
/// for all dl::widget variant types and composite structs.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "DeepLearning_Widget/Core/DeepLearningParamSchemasUIHints.hpp"

#include <rfl/json.hpp>


// ============================================================================
// StaticInputSourceVariant
// ============================================================================

TEST_CASE("StaticInputSourceVariant schema extraction",
          "[dl_widget][param_schema][static_source]") {
    struct Wrapper {
        dl::widget::StaticInputSourceVariant source =
                dl::widget::DataManagerStaticSourceParams{};
    };

    auto schema = extractParameterSchema<Wrapper>();
    REQUIRE(schema.fields.size() == 1);

    auto * f = schema.field("source");
    REQUIRE(f != nullptr);
    CHECK(f->is_variant);
    CHECK(f->type_name == "variant");
    CHECK(f->variant_discriminator == "static_source");
    REQUIRE(f->variant_alternatives.size() == 2);
    CHECK(f->allowed_values.size() == 2);

    SECTION("DataManagerStaticSourceParams alternative") {
        auto const & alt = f->variant_alternatives[0];
        CHECK(alt.tag == "DataManagerStaticSourceParams");
        REQUIRE(alt.schema != nullptr);
        CHECK(alt.schema->fields.size() == 2);
        CHECK(alt.schema->field("data_key") != nullptr);
        CHECK(alt.schema->field("time_offset") != nullptr);
    }

    SECTION("DataBankStaticSourceParams alternative") {
        auto const & alt = f->variant_alternatives[1];
        CHECK(alt.tag == "DataBankStaticSourceParams");
        REQUIRE(alt.schema != nullptr);
        CHECK(alt.schema->fields.size() == 1);
        CHECK(alt.schema->field("bank_entry_id") != nullptr);
    }
}

TEST_CASE("StaticInputSourceVariant JSON round-trip",
          "[dl_widget][param_schema][static_source][roundtrip]") {
    SECTION("DataManager source") {
        dl::widget::DataManagerStaticSourceParams params{
                .data_key = "media/video",
                .time_offset = -5};
        dl::widget::StaticInputSourceVariant var{params};
        auto json = rfl::json::write(var);
        auto result = rfl::json::read<dl::widget::StaticInputSourceVariant>(json);
        REQUIRE(result);
    }

    SECTION("DataBank source") {
        dl::widget::StaticInputSourceVariant var{
                dl::widget::DataBankStaticSourceParams{.bank_entry_id = "ref_1"}};
        auto json = rfl::json::write(var);
        auto result = rfl::json::read<dl::widget::StaticInputSourceVariant>(json);
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
        CHECK(s.field("static_source_kind") != nullptr);
        CHECK(s.field("data_key") != nullptr);
        CHECK(s.field("bank_entry_id") != nullptr);
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
                .static_source_kind = "DataBank",
                .bank_entry_id = "whisker_ref"};
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
// GeneralEncoderModelParams (moved from widget EncoderShapeParams)
// ============================================================================

TEST_CASE("GeneralEncoderModelParams schema is covered in GeneralEncoderConfiguration tests",
          "[dl_widget][param_schema][encoder_shape]") {
    SUCCEED("See tests/DeepLearning/models_v2/GeneralEncoderConfiguration.test.cpp");
}

// ============================================================================
// UIHints annotation tests
// ============================================================================

TEST_CASE("StaticSequenceEntryParams UIHints annotate static_source_kind field",
          "[dl_widget][param_schema][uihints]") {
    auto schema = extractParameterSchema<dl::widget::StaticSequenceEntryParams>();

    auto * f = schema.field("static_source_kind");
    REQUIRE(f != nullptr);
    CHECK(f->display_name == "Source");
    REQUIRE(f->allowed_values.size() == 2);
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
