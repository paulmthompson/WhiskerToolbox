#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "channel_decoding/TensorToFeatureVector.hpp"

#include <ATen/Functions.h>  // at::zeros, at::tensor
#include <ATen/core/Tensor.h>// at::Tensor

#include <stdexcept>

using Catch::Matchers::WithinAbs;

TEST_CASE("TensorToFeatureVector - name and output type",
          "[channel_decoding][TensorToFeatureVector]") {
    dl::TensorToFeatureVector decoder;
    CHECK(decoder.name() == "TensorToFeatureVector");
    CHECK(decoder.outputTypeName() == "std::vector<float>");
}

TEST_CASE("TensorToFeatureVector - unbatched 1D decode",
          "[channel_decoding][TensorToFeatureVector]") {
    dl::TensorToFeatureVector decoder;

    auto tensor = at::tensor({0.1f, 0.2f, 0.3f});
    dl::DecoderContext ctx;
    dl::FeatureVectorDecoderParams params;

    auto const features = decoder.decode(tensor, ctx, params);
    REQUIRE(features.size() == 3);
    CHECK_THAT(features[0], WithinAbs(0.1f, 1e-6f));
    CHECK_THAT(features[1], WithinAbs(0.2f, 1e-6f));
    CHECK_THAT(features[2], WithinAbs(0.3f, 1e-6f));
}

TEST_CASE("TensorToFeatureVector - batched 2D decode",
          "[channel_decoding][TensorToFeatureVector]") {
    dl::TensorToFeatureVector decoder;

    auto tensor = at::zeros({2, 3});
    tensor[0] = at::tensor({1.0f, 2.0f, 3.0f});
    tensor[1] = at::tensor({4.0f, 5.0f, 6.0f});

    dl::DecoderContext ctx;
    dl::FeatureVectorDecoderParams params;

    ctx.batch_index = 0;
    auto const batch0 = decoder.decode(tensor, ctx, params);
    REQUIRE(batch0.size() == 3);
    CHECK_THAT(batch0[0], WithinAbs(1.0f, 1e-6f));
    CHECK_THAT(batch0[2], WithinAbs(3.0f, 1e-6f));

    ctx.batch_index = 1;
    auto const batch1 = decoder.decode(tensor, ctx, params);
    REQUIRE(batch1.size() == 3);
    CHECK_THAT(batch1[0], WithinAbs(4.0f, 1e-6f));
    CHECK_THAT(batch1[2], WithinAbs(6.0f, 1e-6f));
}

TEST_CASE("TensorToFeatureVector - rejects 0D tensor",
          "[channel_decoding][TensorToFeatureVector]") {
    dl::TensorToFeatureVector decoder;

    // at::tensor(1.0f) is 1D [1] in LibTorch; scalar_tensor is the true 0D case.
    auto tensor = at::scalar_tensor(1.0f);
    dl::DecoderContext ctx;
    dl::FeatureVectorDecoderParams params;

    CHECK_THROWS_AS(decoder.decode(tensor, ctx, params), std::invalid_argument);
}

TEST_CASE("TensorToFeatureVector - rejects higher-rank tensor",
          "[channel_decoding][TensorToFeatureVector]") {
    dl::TensorToFeatureVector decoder;

    auto tensor = at::zeros({1, 4, 8, 8});
    dl::DecoderContext ctx;
    dl::FeatureVectorDecoderParams params;

    CHECK_THROWS_AS(decoder.decode(tensor, ctx, params), std::invalid_argument);
}

TEST_CASE("TensorToFeatureVector - rejects out-of-range batch index",
          "[channel_decoding][TensorToFeatureVector]") {
    dl::TensorToFeatureVector decoder;

    auto tensor = at::zeros({1, 4});
    dl::DecoderContext ctx;
    ctx.batch_index = 1;
    dl::FeatureVectorDecoderParams params;

    CHECK_THROWS_AS(decoder.decode(tensor, ctx, params), std::out_of_range);
}
