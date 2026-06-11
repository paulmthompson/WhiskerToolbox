/// @file DeepLearningState.test.cpp
/// @brief Tests for DeepLearningState serialization.

#include <catch2/catch_test_macros.hpp>

#include "DeepLearning_Widget/Core/DeepLearningState.hpp"

TEST_CASE("DeepLearningState round-trips post_encoder_params",
          "[dl_widget][deep_learning_state]") {
    DeepLearningState state;

    dl::widget::PostEncoderSlotParams params;
    dl::SpatialPointModuleParams spatial;
    spatial.point_key = "points/query";
    params.module = spatial;
    state.setPostEncoderParams(params);

    auto const json = state.toJson();
    DeepLearningState restored;
    REQUIRE(restored.fromJson(json));

    auto const & restored_params = restored.postEncoderParams();
    restored_params.module.visit([&](auto const & mod) {
        using T = std::decay_t<decltype(mod)>;
        CHECK(std::is_same_v<T, dl::SpatialPointModuleParams>);
        if constexpr (std::is_same_v<T, dl::SpatialPointModuleParams>) {
            CHECK(mod.point_key == "points/query");
        }
    });
}
