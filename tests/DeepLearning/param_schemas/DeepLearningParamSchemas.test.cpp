/// @file DeepLearningParamSchemas.test.cpp
/// @brief Tests for memory-frame parameter schemas and tagged unions.

#include <catch2/catch_test_macros.hpp>

#include "DeepLearning/bindings/BindingParamSchemas.hpp"

#include <rfl/json.hpp>

TEST_CASE("StaticInputSourceVariant schema extraction",
          "[dl_bindings][param_schema][static_source]") {
    struct Wrapper {
        dl::StaticInputSourceVariant source = dl::DataManagerStaticSource{};
    };

    auto schema = extractParameterSchema<Wrapper>();
    REQUIRE(schema.fields.size() == 1);

    auto * f = schema.field("source");
    REQUIRE(f != nullptr);
    CHECK(f->is_variant);
    CHECK(f->variant_discriminator == "static_source");
    REQUIRE(f->variant_alternatives.size() == 2);
}

TEST_CASE("StaticInputSourceVariant JSON round-trip",
          "[dl_bindings][param_schema][static_source][roundtrip]") {
    dl::StaticInputSourceVariant var{
            dl::DataManagerStaticSource{.data_key = "media/video", .time_offset = -5}};
    auto json = rfl::json::write(var);
    auto result = rfl::json::read<dl::StaticInputSourceVariant>(json);
    REQUIRE(result);
}

TEST_CASE("RecurrentInitVariant schema extraction",
          "[dl_bindings][param_schema][recurrent_init]") {
    struct Wrapper {
        dl::RecurrentInitVariant init = dl::ZerosInit{};
    };

    auto schema = extractParameterSchema<Wrapper>();
    auto * f = schema.field("init");
    REQUIRE(f != nullptr);
    CHECK(f->is_variant);
    CHECK(f->variant_discriminator == "init_mode");
    REQUIRE(f->variant_alternatives.size() == 3);
}

TEST_CASE("RecurrentInitVariant JSON round-trip",
          "[dl_bindings][param_schema][recurrent_init][roundtrip]") {
    dl::RecurrentInitVariant var{dl::StaticCaptureInit{
            .data_key = "media/video_1", .frame = 10}};
    auto json = rfl::json::write(var);
    auto result = rfl::json::read<dl::RecurrentInitVariant>(json);
    REQUIRE(result);
}

TEST_CASE("MemoryFrameBinding JSON round-trip",
          "[dl_bindings][param_schema][memory_frame][roundtrip]") {
    dl::MemoryFrameBinding binding;
    binding.slot_name = "memory_masks";
    binding.memory_index = 0;
    binding.frame = dl::RecurrentFrameSource{
            .output_slot_name = "mask_out",
            .init = dl::ZerosInit{}};

    auto json = rfl::json::write(binding);
    auto result = rfl::json::read<dl::MemoryFrameBinding>(json);
    REQUIRE(result);
    CHECK(result.value().slot_name == "memory_masks");
    CHECK(dl::recurrentOutputSlot(result.value()) == "mask_out");
}

TEST_CASE("StaticFrameSourceForm UIHints annotate source field",
          "[dl_bindings][param_schema][uihints]") {
    auto schema = extractParameterSchema<dl::StaticFrameSourceForm>();
    auto * f = schema.field("source");
    REQUIRE(f != nullptr);
    CHECK(f->display_name == "Source");
}

TEST_CASE("RecurrentFrameSourceForm UIHints annotate init field",
          "[dl_bindings][param_schema][uihints]") {
    auto schema = extractParameterSchema<dl::RecurrentFrameSourceForm>();
    auto * f = schema.field("init");
    REQUIRE(f != nullptr);
    CHECK(f->display_name == "Init Mode");
}
