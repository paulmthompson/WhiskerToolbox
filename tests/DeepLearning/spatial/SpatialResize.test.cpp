/**
 * @file SpatialResize.test.cpp
 * @brief Unit tests for pixel-center spatial mapping in DeepLearning.
 */

#include "spatial/SpatialResize.hpp"

#include "channel_decoding/TensorToMask2D.hpp"
#include "channel_encoding/Mask2DEncoder.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"

#include <ATen/Functions.h>
#include <ATen/core/Tensor.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace {

struct ProbeExpectation {
    int image_x;
    int image_y;
    int tensor_x;
    int tensor_y;
    float continuous_x;
    float continuous_y;
};

std::vector<ProbeExpectation> neurosam_probe_expectations() {
    return {
            {297, 161, 119, 86, 118.50f, 85.63f},
            {301, 178, 120, 95, 120.10f, 94.70f},
            {228, 230, 91, 122, 90.90f, 122.43f},
            {209, 245, 83, 130, 83.30f, 130.43f},
            {281, 188, 112, 100, 112.10f, 100.03f},
    };
}

Mask2D mask_from_single_pixel(uint32_t x, uint32_t y) {
    return Mask2D{Point2D<uint32_t>{x, y}};
}

int nearest_recovered_pixel(Mask2D const & mask, int original_x, int original_y) {
    int best_x = original_x;
    int best_y = original_y;
    int best_dist = std::numeric_limits<int>::max();

    for (auto const & point: mask) {
        int const dx = static_cast<int>(point.x) - original_x;
        int const dy = static_cast<int>(point.y) - original_y;
        int const dist = dx * dx + dy * dy;
        if (dist < best_dist) {
            best_dist = dist;
            best_x = static_cast<int>(point.x);
            best_y = static_cast<int>(point.y);
        }
    }

    return best_x == original_x && best_y == original_y ? 0 : 1;
}

}// namespace

TEST_CASE("SpatialResize - continuous image/tensor round-trip at integer probes",
          "[spatial][SpatialResize]") {
    ImageSize const image_size{640, 480};
    int const tensor_w = 256;
    int const tensor_h = 256;

    for (auto const & probe: neurosam_probe_expectations()) {
        float const tx = dl::spatial::continuous_image_to_tensor(
                static_cast<float>(probe.image_x), image_size.width, tensor_w);
        float const ty = dl::spatial::continuous_image_to_tensor(
                static_cast<float>(probe.image_y), image_size.height, tensor_h);

        CHECK_THAT(tx, WithinAbs(probe.continuous_x, 0.01f));
        CHECK_THAT(ty, WithinAbs(probe.continuous_y, 0.01f));

        float const rx = dl::spatial::continuous_tensor_to_image(tx, tensor_w, image_size.width);
        float const ry = dl::spatial::continuous_tensor_to_image(ty, tensor_h, image_size.height);

        CHECK_THAT(rx, WithinAbs(static_cast<float>(probe.image_x), 1e-4f));
        CHECK_THAT(ry, WithinAbs(static_cast<float>(probe.image_y), 1e-4f));
    }
}

TEST_CASE("SpatialResize - discrete encode and decode agree at probe points",
          "[spatial][SpatialResize]") {
    ImageSize const image_size{640, 480};
    int const tensor_w = 256;
    int const tensor_h = 256;

    for (auto const & probe: neurosam_probe_expectations()) {
        int const encode_x = dl::spatial::discrete_image_to_tensor(
                probe.image_x, image_size.width, tensor_w);
        int const encode_y = dl::spatial::discrete_image_to_tensor(
                probe.image_y, image_size.height, tensor_h);

        int const decode_x = dl::spatial::discrete_dest_to_source(
                probe.image_x, tensor_w, image_size.width);
        int const decode_y = dl::spatial::discrete_dest_to_source(
                probe.image_y, tensor_h, image_size.height);

        CHECK(encode_x == probe.tensor_x);
        CHECK(encode_y == probe.tensor_y);
        CHECK(decode_x == probe.tensor_x);
        CHECK(decode_y == probe.tensor_y);
    }
}

TEST_CASE("SpatialResize - mask encode/decode round-trip at probe points",
          "[spatial][SpatialResize][roundtrip]") {
    dl::Mask2DEncoder const encoder;
    dl::TensorToMask2D const decoder;

    ImageSize const image_size{640, 480};
    ImageSize const tensor_size{256, 256};

    dl::Mask2DEncoderParams enc_params;
    enc_params.mode = dl::RasterMode::Binary;

    dl::MaskDecoderParams dec_params;
    dec_params.threshold = 0.5f;

    for (auto const & probe: neurosam_probe_expectations()) {
        auto tensor = at::zeros({1, 1, tensor_size.height, tensor_size.width});

        dl::EncoderContext enc_ctx;
        enc_ctx.target_channel = 0;
        enc_ctx.batch_index = 0;
        enc_ctx.height = tensor_size.height;
        enc_ctx.width = tensor_size.width;

        encoder.encode(
                mask_from_single_pixel(static_cast<uint32_t>(probe.image_x),
                                       static_cast<uint32_t>(probe.image_y)),
                image_size,
                tensor,
                enc_ctx,
                enc_params);

        dl::DecoderContext dec_ctx;
        dec_ctx.source_channel = 0;
        dec_ctx.batch_index = 0;
        dec_ctx.height = tensor_size.height;
        dec_ctx.width = tensor_size.width;
        dec_ctx.target_image_size = image_size;

        auto const decoded = decoder.decode(tensor, dec_ctx, dec_params);
        REQUIRE(!decoded.empty());

        bool contains_original = false;
        for (auto const & point: decoded) {
            if (point.x == static_cast<uint32_t>(probe.image_x) &&
                point.y == static_cast<uint32_t>(probe.image_y)) {
                contains_original = true;
                break;
            }
        }

        CHECK(contains_original);
        CHECK(nearest_recovered_pixel(decoded, probe.image_x, probe.image_y) == 0);
    }
}
