/// @file PostEncoderModules.test.cpp
/// @brief Unit tests for all post-encoder module implementations.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "post_encoder/GlobalAvgPoolModule.hpp"
#include "post_encoder/PostEncoderModuleFactory.hpp"
#include "post_encoder/PostEncoderPipeline.hpp"
#include "post_encoder/SpatialPointExtractModule.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/points.hpp"

#include <torch/torch.h>

#include <cmath>
#include <memory>
#include <vector>

using Catch::Matchers::WithinAbs;

// ─────────────────────────────────────────────────────────────────────────────
// GlobalAvgPoolModule
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("GlobalAvgPoolModule - name", "[post_encoder][GlobalAvgPoolModule]")
{
    dl::GlobalAvgPoolModule mod;
    CHECK(mod.name() == "global_avg_pool");
}

TEST_CASE("GlobalAvgPoolModule - output shape [B,C,H,W] -> [B,C]",
          "[post_encoder][GlobalAvgPoolModule]")
{
    dl::GlobalAvgPoolModule mod;

    // outputShape on raw shape vector
    auto shape = mod.outputShape({4, 8, 16});// C=4, H=8, W=16
    REQUIRE(shape.size() == 1);
    CHECK(shape[0] == 4);
}

TEST_CASE("GlobalAvgPoolModule - apply reduces spatial dimensions",
          "[post_encoder][GlobalAvgPoolModule]")
{
    dl::GlobalAvgPoolModule mod;

    // [B=2, C=3, H=4, W=4] — constant value 2.0 per channel
    auto feat = torch::full({2, 3, 4, 4}, 2.0f);
    auto out = mod.apply(feat);

    REQUIRE(out.dim() == 2);
    CHECK(out.size(0) == 2);
    CHECK(out.size(1) == 3);

    // All values should average to 2.0
    auto cpu = out.cpu().contiguous();
    auto acc = cpu.accessor<float, 2>();
    for (int b = 0; b < 2; ++b) {
        for (int c = 0; c < 3; ++c) {
            CHECK_THAT(acc[b][c], WithinAbs(2.0f, 1e-5f));
        }
    }
}

TEST_CASE("GlobalAvgPoolModule - channels averaged correctly",
          "[post_encoder][GlobalAvgPoolModule]")
{
    dl::GlobalAvgPoolModule mod;

    // [B=1, C=2, H=2, W=2]
    // Channel 0: all 1.0
    // Channel 1: all 5.0
    auto feat = torch::zeros({1, 2, 2, 2});
    feat.select(1, 0).fill_(1.0f);
    feat.select(1, 1).fill_(5.0f);

    auto out = mod.apply(feat);
    REQUIRE(out.dim() == 2);
    CHECK(out.size(0) == 1);
    CHECK(out.size(1) == 2);

    auto cpu = out.cpu().contiguous();
    auto acc = cpu.accessor<float, 2>();
    CHECK_THAT(acc[0][0], WithinAbs(1.0f, 1e-5f));
    CHECK_THAT(acc[0][1], WithinAbs(5.0f, 1e-5f));
}

TEST_CASE("GlobalAvgPoolModule - single spatial pixel passthrough",
          "[post_encoder][GlobalAvgPoolModule]")
{
    dl::GlobalAvgPoolModule mod;

    // [B=1, C=4, H=1, W=1]
    auto feat = torch::arange(4.0f).view({1, 4, 1, 1});
    auto out = mod.apply(feat);
    REQUIRE(out.dim() == 2);

    auto cpu = out.cpu().contiguous();
    auto acc = cpu.accessor<float, 2>();
    for (int c = 0; c < 4; ++c) {
        CHECK_THAT(acc[0][c], WithinAbs(static_cast<float>(c), 1e-5f));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SpatialPointExtractModule
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("SpatialPointExtractModule - name",
          "[post_encoder][SpatialPointExtractModule]")
{
    ImageSize const src{10, 10};
    dl::SpatialPointExtractModule mod{src, dl::InterpolationMode::Nearest};
    CHECK(mod.name() == "spatial_point");
}

TEST_CASE("SpatialPointExtractModule - output shape [B,C,H,W] -> [B,C]",
          "[post_encoder][SpatialPointExtractModule]")
{
    ImageSize const src{64, 64};
    dl::SpatialPointExtractModule mod{src, dl::InterpolationMode::Nearest};

    auto shape = mod.outputShape({16, 8, 8});// C=16, H=8, W=8
    REQUIRE(shape.size() == 1);
    CHECK(shape[0] == 16);
}

TEST_CASE("SpatialPointExtractModule - nearest: center point extracts correct channel",
          "[post_encoder][SpatialPointExtractModule]")
{
    // Source image 10×10, feature map 10×10 (identity scale)
    ImageSize const src{10, 10};
    dl::SpatialPointExtractModule mod{src, dl::InterpolationMode::Nearest};

    // [B=1, C=3, H=10, W=10]
    // Set all pixels of channel 1 to 7.0 so we always extract 7.0 from that channel
    auto feat = torch::zeros({1, 3, 10, 10});
    feat.select(1, 1).fill_(7.0f);

    // Query center of image
    mod.setPoint({5.0f, 5.0f});
    auto out = mod.apply(feat);

    REQUIRE(out.dim() == 2);
    CHECK(out.size(0) == 1);
    CHECK(out.size(1) == 3);

    auto cpu = out.cpu().contiguous();
    auto acc = cpu.accessor<float, 2>();
    CHECK_THAT(acc[0][0], WithinAbs(0.0f, 1e-5f));
    CHECK_THAT(acc[0][1], WithinAbs(7.0f, 1e-5f));
    CHECK_THAT(acc[0][2], WithinAbs(0.0f, 1e-5f));
}

TEST_CASE("SpatialPointExtractModule - nearest: top-left corner",
          "[post_encoder][SpatialPointExtractModule]")
{
    ImageSize const src{4, 4};
    dl::SpatialPointExtractModule mod{src, dl::InterpolationMode::Nearest};

    // [B=1, C=1, H=4, W=4]: pixel (0,0)=3.0, all others=0.0
    auto feat = torch::zeros({1, 1, 4, 4});
    feat[0][0][0][0] = 3.0f;

    mod.setPoint({0.0f, 0.0f});
    auto out = mod.apply(feat);

    auto cpu = out.cpu().contiguous();
    auto acc = cpu.accessor<float, 2>();
    CHECK_THAT(acc[0][0], WithinAbs(3.0f, 1e-5f));
}

TEST_CASE("SpatialPointExtractModule - nearest: clamping beyond boundary",
          "[post_encoder][SpatialPointExtractModule]")
{
    ImageSize const src{4, 4};
    dl::SpatialPointExtractModule mod{src, dl::InterpolationMode::Nearest};

    auto feat = torch::zeros({1, 1, 4, 4});
    // Bottom-right last pixel
    feat[0][0][3][3] = 9.0f;

    // Point beyond image edge should clamp
    mod.setPoint({100.0f, 100.0f});
    auto out = mod.apply(feat);

    auto cpu = out.cpu().contiguous();
    auto acc = cpu.accessor<float, 2>();
    CHECK_THAT(acc[0][0], WithinAbs(9.0f, 1e-5f));
}

TEST_CASE("SpatialPointExtractModule - bilinear interpolation: midpoint",
          "[post_encoder][SpatialPointExtractModule]")
{
    // 4×4 source, 4×4 feature map
    ImageSize const src{4, 4};
    dl::SpatialPointExtractModule mod{src, dl::InterpolationMode::Bilinear};

    // [B=1, C=1, H=4, W=4]: ramp across x
    auto feat = torch::zeros({1, 1, 4, 4});
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            feat[0][0][y][x] = static_cast<float>(x);
        }
    }

    // Query at x=1.5 (midpoint between columns 1 and 2) → bilinear ≈ 1.5
    mod.setPoint({1.5f, 0.0f});
    auto out = mod.apply(feat);

    auto cpu = out.cpu().contiguous();
    auto acc = cpu.accessor<float, 2>();
    CHECK_THAT(acc[0][0], WithinAbs(1.5f, 0.1f));
}

// ─────────────────────────────────────────────────────────────────────────────
// PostEncoderPipeline
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("PostEncoderPipeline - empty pipeline is pass-through",
          "[post_encoder][PostEncoderPipeline]")
{
    dl::PostEncoderPipeline pipeline;

    auto feat = torch::ones({1, 4, 8, 8});
    auto out = pipeline.apply(feat);

    CHECK(out.equal(feat));
}

TEST_CASE("PostEncoderPipeline - single module delegated correctly",
          "[post_encoder][PostEncoderPipeline]")
{
    dl::PostEncoderPipeline pipeline;
    pipeline.add(std::make_unique<dl::GlobalAvgPoolModule>());

    auto feat = torch::full({1, 3, 4, 4}, 2.5f);
    auto out = pipeline.apply(feat);

    REQUIRE(out.dim() == 2);
    CHECK(out.size(0) == 1);
    CHECK(out.size(1) == 3);

    auto cpu = out.cpu().contiguous();
    auto acc = cpu.accessor<float, 2>();
    CHECK_THAT(acc[0][0], WithinAbs(2.5f, 1e-5f));
}

TEST_CASE("PostEncoderPipeline - outputShape chains sequentially",
          "[post_encoder][PostEncoderPipeline]")
{
    dl::PostEncoderPipeline pipeline;
    pipeline.add(std::make_unique<dl::GlobalAvgPoolModule>());

    // [C=8, H=4, W=4] → [C=8] after GlobalAvgPool
    auto shape = pipeline.outputShape({8, 4, 4});
    REQUIRE(shape.size() == 1);
    CHECK(shape[0] == 8);
}

TEST_CASE("PostEncoderPipeline - empty pipeline name",
          "[post_encoder][PostEncoderPipeline]")
{
    dl::PostEncoderPipeline pipeline;
    CHECK(pipeline.name() == "pipeline");
}

// ─────────────────────────────────────────────────────────────────────────────
// PostEncoderModuleFactory
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("PostEncoderModuleFactory - none returns nullptr",
          "[post_encoder][PostEncoderModuleFactory]")
{
    dl::PostEncoderModuleParams params;
    params.source_image_size = {64, 64};

    auto mod = dl::PostEncoderModuleFactory::create("none", params);
    CHECK(mod == nullptr);

    auto mod2 = dl::PostEncoderModuleFactory::create("", params);
    CHECK(mod2 == nullptr);
}

TEST_CASE("PostEncoderModuleFactory - global_avg_pool creates GlobalAvgPoolModule",
          "[post_encoder][PostEncoderModuleFactory]")
{
    dl::PostEncoderModuleParams params;
    params.source_image_size = {64, 64};

    auto mod = dl::PostEncoderModuleFactory::create("global_avg_pool", params);
    REQUIRE(mod != nullptr);
    CHECK(mod->name() == "global_avg_pool");
}

TEST_CASE("PostEncoderModuleFactory - spatial_point creates SpatialPointExtractModule",
          "[post_encoder][PostEncoderModuleFactory]")
{
    dl::PostEncoderModuleParams params;
    params.source_image_size = {32, 32};
    params.interpolation = "nearest";

    auto mod = dl::PostEncoderModuleFactory::create("spatial_point", params);
    REQUIRE(mod != nullptr);
    CHECK(mod->name() == "spatial_point");
}

TEST_CASE("PostEncoderModuleFactory - unknown key returns nullptr",
          "[post_encoder][PostEncoderModuleFactory]")
{
    dl::PostEncoderModuleParams params;
    params.source_image_size = {64, 64};

    auto mod = dl::PostEncoderModuleFactory::create("does_not_exist", params);
    CHECK(mod == nullptr);
}
