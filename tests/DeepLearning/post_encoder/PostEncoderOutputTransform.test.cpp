/// @file PostEncoderOutputTransform.test.cpp
/// @brief Tests for post-encoder output transform helpers.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "post_encoder/GlobalAvgPoolModule.hpp"
#include "post_encoder/PostEncoderOutputTransform.hpp"

#include "models_v2/TensorSlotDescriptor.hpp"

#include <torch/torch.h>

TEST_CASE("applyPostEncoderToFirstOutputSlot - pass-through when module is null",
          "[post_encoder][PostEncoderOutputTransform]") {
    std::vector<dl::TensorSlotDescriptor> const slots{
            {.name = "features",
             .shape = {4, 8, 8},
             .description = {},
             .recommended_encoder = {},
             .recommended_decoder = {},
             .is_static = false,
             .is_boolean_mask = false,
             .dtype = dl::TensorDType::Float32,
             .sequence_dim = -1},
    };

    auto feat = at::ones({1, 4, 8, 8});
    std::unordered_map<std::string, at::Tensor> outputs{{"features", feat}};

    auto const result =
            dl::applyPostEncoderToFirstOutputSlot(outputs, slots, nullptr);

    REQUIRE(result.at("features").equal(feat));
}

TEST_CASE("applyPostEncoderToFirstOutputSlot - transforms first slot only",
          "[post_encoder][PostEncoderOutputTransform]") {
    std::vector<dl::TensorSlotDescriptor> const slots{
            {.name = "features",
             .shape = {3, 4, 4},
             .description = {},
             .recommended_encoder = {},
             .recommended_decoder = {},
             .is_static = false,
             .is_boolean_mask = false,
             .dtype = dl::TensorDType::Float32,
             .sequence_dim = -1},
            {.name = "other",
             .shape = {2, 2, 2},
             .description = {},
             .recommended_encoder = {},
             .recommended_decoder = {},
             .is_static = false,
             .is_boolean_mask = false,
             .dtype = dl::TensorDType::Float32,
             .sequence_dim = -1},
    };

    auto feat = at::full({1, 3, 4, 4}, 2.0f);
    auto other = at::full({1, 2, 2, 2}, 7.0f);
    std::unordered_map<std::string, at::Tensor> outputs{
            {"features", feat},
            {"other", other},
    };

    dl::GlobalAvgPoolModule pool;
    auto const result =
            dl::applyPostEncoderToFirstOutputSlot(outputs, slots, &pool);

    REQUIRE(result.at("features").dim() == 2);
    CHECK(result.at("features").size(1) == 3);
    CHECK(result.at("other").equal(other));
}

TEST_CASE("effectiveOutputSlots - pass-through when module is null",
          "[post_encoder][PostEncoderOutputTransform]") {
    std::vector<dl::TensorSlotDescriptor> const raw{
            {.name = "features",
             .shape = {384, 7, 7},
             .description = {},
             .recommended_encoder = {},
             .recommended_decoder = {},
             .is_static = false,
             .is_boolean_mask = false,
             .dtype = dl::TensorDType::Float32,
             .sequence_dim = -1},
    };

    auto const slots = dl::effectiveOutputSlots(raw, nullptr);
    REQUIRE(slots.size() == 1);
    CHECK(slots[0].shape == raw[0].shape);
}

TEST_CASE("effectiveOutputSlots - propagates shape through GlobalAvgPool",
          "[post_encoder][PostEncoderOutputTransform]") {
    std::vector<dl::TensorSlotDescriptor> const raw{
            {.name = "features",
             .shape = {8, 4, 4},
             .description = {},
             .recommended_encoder = {},
             .recommended_decoder = {},
             .is_static = false,
             .is_boolean_mask = false,
             .dtype = dl::TensorDType::Float32,
             .sequence_dim = -1},
    };

    dl::GlobalAvgPoolModule pool;
    auto const slots = dl::effectiveOutputSlots(raw, &pool);

    REQUIRE(slots.size() == 1);
    REQUIRE(slots[0].shape.size() == 1);
    CHECK(slots[0].shape[0] == 8);
}
