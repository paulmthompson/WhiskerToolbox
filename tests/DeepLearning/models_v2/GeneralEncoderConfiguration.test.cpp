/**
 * @file GeneralEncoderConfiguration.test.cpp
 * @brief Tests for GeneralEncoderModel configuration helpers.
 */

#include <catch2/catch_test_macros.hpp>

#include "models_v2/general_encoder/GeneralEncoderConfiguration.hpp"
#include "models_v2/general_encoder/GeneralEncoderModel.hpp"
#include "models_v2/general_encoder/GeneralEncoderModelParams.hpp"
#include "models_v2/general_encoder/GeneralEncoderParamSchemas.hpp"

#include "ParameterSchema/ParameterSchema.hpp"

#include <rfl/json.hpp>

TEST_CASE("GeneralEncoderModelParams schema extraction",
          "[dl][general_encoder][configuration]") {
    auto schema = extractParameterSchema<dl::GeneralEncoderModelParams>();
    CHECK(schema.fields.size() == 4);

    auto * height = schema.field("input_height");
    REQUIRE(height != nullptr);
    CHECK(height->tooltip ==
          "Input image height in pixels (resized to this before encoding)");
}

TEST_CASE("GeneralEncoderModelParams JSON round-trip",
          "[dl][general_encoder][configuration][roundtrip]") {
    dl::GeneralEncoderModelParams params;
    params.shape_applied = true;
    params.input_height = 320;
    params.input_width = 256;
    params.output_shape = "768,16,16";

    auto const json = rfl::json::write(params);
    auto result = rfl::json::read<dl::GeneralEncoderModelParams>(json);
    REQUIRE(result);
    auto const & restored = result.value();
    CHECK(restored.shape_applied);
    CHECK(restored.input_height == 320);
    CHECK(restored.input_width == 256);
    CHECK(restored.output_shape == "768,16,16");
}

TEST_CASE("generalEncoderParamsForForm uses defaults for unset dimensions",
          "[dl][general_encoder][configuration]") {
    dl::GeneralEncoderModelParams stored;
    auto form = dl::generalEncoderParamsForForm(stored);
    CHECK(form.input_height == 224);
    CHECK(form.input_width == 224);
}

TEST_CASE("applyGeneralEncoderConfiguration updates model slots",
          "[dl][general_encoder][configuration]") {
    dl::GeneralEncoderModel model;
    dl::GeneralEncoderModelParams params;
    params.shape_applied = true;
    params.input_height = 512;
    params.input_width = 384;
    params.output_shape = "768,16,16";

    dl::applyGeneralEncoderConfiguration(model, params);

    auto const inputs = model.inputSlots();
    REQUIRE(inputs.size() == 1);
    CHECK(inputs[0].shape[1] == 512);
    CHECK(inputs[0].shape[2] == 384);

    auto const outputs = model.outputSlots();
    REQUIRE(outputs.size() == 1);
    CHECK(outputs[0].shape == std::vector<int64_t>({768, 16, 16}));
}

TEST_CASE("generalEncoderConfigurationComplete requires shape_applied",
          "[dl][general_encoder][configuration]") {
    dl::GeneralEncoderModelParams params;
    params.input_height = 224;
    params.input_width = 224;
    CHECK_FALSE(dl::generalEncoderConfigurationComplete(params));

    params.shape_applied = true;
    CHECK(dl::generalEncoderConfigurationComplete(params));
}

TEST_CASE("parseGeneralEncoderOutputShape parses comma-separated values",
          "[dl][general_encoder][configuration]") {
    auto shape = dl::parseGeneralEncoderOutputShape("768,16,16");
    REQUIRE(shape.size() == 3);
    CHECK(shape[0] == 768);
    CHECK(shape[1] == 16);
    CHECK(shape[2] == 16);
}
