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
