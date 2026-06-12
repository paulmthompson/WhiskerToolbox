/// @file DeepLearningState.test.cpp
/// @brief Tests for DeepLearningState serialization.

#include <catch2/catch_test_macros.hpp>

#include "DeepLearning_Widget/Core/DeepLearningState.hpp"

#include "DeepLearning/post_encoder/PostEncoderModuleParams.hpp"

#include <rfl/json.hpp>

#include <type_traits>

TEST_CASE("DeepLearningState round-trips post_encoder_params",
          "[dl_widget][deep_learning_state]") {
    DeepLearningState state;

    dl::SpatialPointModuleParams spatial;
    spatial.point_key = "points/query";

    dl::widget::PostEncoderSlotParams params;
    params.module_key = "spatial_point";
    params.parameters_json = rfl::json::write(spatial);
    state.setPostEncoderParams(params);

    auto const json = state.toJson();
    DeepLearningState restored;
    REQUIRE(restored.fromJson(json));

    auto const & restored_params = restored.postEncoderParams();
    CHECK(restored_params.module_key == "spatial_point");

    auto const parsed =
            rfl::json::read<dl::SpatialPointModuleParams>(restored_params.parameters_json);
    REQUIRE(parsed);
    CHECK(parsed.value().point_key == "points/query");
}

TEST_CASE("DeepLearningState round-trips output_bindings with nested decoder",
          "[dl_widget][deep_learning_state]") {
    DeepLearningState state;

    OutputBindingData binding;
    binding.slot_name = "mask_out";
    binding.data_key = "masks/result";
    binding.decoder = dl::MaskDecoderParams{.threshold = 0.65f};
    state.setOutputBindings({binding});

    auto const json = state.toJson();
    DeepLearningState restored;
    REQUIRE(restored.fromJson(json));

    auto const & restored_bindings = restored.outputBindings();
    REQUIRE(restored_bindings.size() == 1);
    CHECK(restored_bindings[0].slot_name == "mask_out");
    CHECK(restored_bindings[0].data_key == "masks/result");
    restored_bindings[0].decoder.visit([&](auto const & dec) {
        using T = std::decay_t<decltype(dec)>;
        if constexpr (std::is_same_v<T, dl::MaskDecoderParams>) {
            CHECK(dec.threshold == 0.65f);
        }
    });
}
