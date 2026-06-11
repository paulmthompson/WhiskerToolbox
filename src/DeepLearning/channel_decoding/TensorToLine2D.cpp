/**
 * @file TensorToLine2D.cpp
 * @brief Implementation of the spatial line decoder.
 */

#include "TensorToLine2D.hpp"

#include <ATen/core/Tensor.h>// at::Tensor
#include <spdlog/spdlog.h>
#include <torch/types.h>// kCPU, kFloat32

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace dl {

namespace {

/**
 * @brief Validate tensor shape and decoder context for spatial line decoding.
 *
 * @pre tensor is defined (enforced via assert)
 */
void validateTensorToLine2DInput(at::Tensor const & tensor, DecoderContext const & ctx) {
    assert(tensor.defined() && "TensorToLine2D: tensor must be defined");

    if (tensor.dim() != 4) {
        auto const message =
                "TensorToLine2D: expected 4D tensor [B, C, H, W], got dim=" +
                std::to_string(tensor.dim());
        spdlog::debug("[TensorToLine2D] {}", message);
        throw std::invalid_argument(message);
    }

    if (ctx.height <= 0 || ctx.width <= 0) {
        auto const message = "TensorToLine2D: ctx.height and ctx.width must be > 0";
        spdlog::debug("[TensorToLine2D] {}", message);
        throw std::invalid_argument(message);
    }

    auto const batch_size = tensor.size(0);
    auto const channel_count = tensor.size(1);
    auto const tensor_height = tensor.size(2);
    auto const tensor_width = tensor.size(3);

    if (ctx.batch_index < 0 || static_cast<int64_t>(ctx.batch_index) >= batch_size) {
        auto const message = "TensorToLine2D: batch_index " +
                             std::to_string(ctx.batch_index) + " out of range [0, " +
                             std::to_string(batch_size) + ")";
        spdlog::debug("[TensorToLine2D] {}", message);
        throw std::out_of_range(message);
    }

    if (ctx.source_channel < 0 ||
        static_cast<int64_t>(ctx.source_channel) >= channel_count) {
        auto const message = "TensorToLine2D: source_channel " +
                             std::to_string(ctx.source_channel) + " out of range [0, " +
                             std::to_string(channel_count) + ")";
        spdlog::debug("[TensorToLine2D] {}", message);
        throw std::out_of_range(message);
    }

    if (tensor_height != ctx.height || tensor_width != ctx.width) {
        auto const message = "TensorToLine2D: tensor spatial dims [" +
                             std::to_string(tensor_height) + ", " +
                             std::to_string(tensor_width) + "] do not match DecoderContext [" +
                             std::to_string(ctx.height) + ", " + std::to_string(ctx.width) + "]";
        spdlog::debug("[TensorToLine2D] {}", message);
        throw std::invalid_argument(message);
    }
}

/**
 * @brief Zhang-Suen thinning algorithm.
 *
 * Thins a binary image in-place to a 1-pixel-wide skeleton.
 * grid is row-major: grid[y * w + x], with values 0 or 1.
 */
void zhang_suen_thinning(std::vector<uint8_t> & grid, int const w, int const h) {
    bool changed = true;

    auto at = [&](int x, int y) -> uint8_t {
        if (x < 0 || x >= w || y < 0 || y >= h) return 0;
        return grid[y * w + x];
    };

    while (changed) {
        changed = false;

        std::vector<int> to_remove;
        for (int y = 1; y < h - 1; ++y) {
            for (int x = 1; x < w - 1; ++x) {
                if (grid[y * w + x] == 0) continue;

                uint8_t const p2 = at(x, y - 1);
                uint8_t const p3 = at(x + 1, y - 1);
                uint8_t const p4 = at(x + 1, y);
                uint8_t const p5 = at(x + 1, y + 1);
                uint8_t const p6 = at(x, y + 1);
                uint8_t const p7 = at(x - 1, y + 1);
                uint8_t const p8 = at(x - 1, y);
                uint8_t const p9 = at(x - 1, y - 1);

                int const B = p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9;
                if (B < 2 || B > 6) continue;

                int A = 0;
                if (p2 == 0 && p3 == 1) ++A;
                if (p3 == 0 && p4 == 1) ++A;
                if (p4 == 0 && p5 == 1) ++A;
                if (p5 == 0 && p6 == 1) ++A;
                if (p6 == 0 && p7 == 1) ++A;
                if (p7 == 0 && p8 == 1) ++A;
                if (p8 == 0 && p9 == 1) ++A;
                if (p9 == 0 && p2 == 1) ++A;
                if (A != 1) continue;

                if (p2 * p4 * p6 != 0) continue;
                if (p4 * p6 * p8 != 0) continue;

                to_remove.push_back(y * w + x);
            }
        }
        for (int const idx: to_remove) {
            grid[idx] = 0;
            changed = true;
        }

        to_remove.clear();
        for (int y = 1; y < h - 1; ++y) {
            for (int x = 1; x < w - 1; ++x) {
                if (grid[y * w + x] == 0) continue;

                uint8_t const p2 = at(x, y - 1);
                uint8_t const p3 = at(x + 1, y - 1);
                uint8_t const p4 = at(x + 1, y);
                uint8_t const p5 = at(x + 1, y + 1);
                uint8_t const p6 = at(x, y + 1);
                uint8_t const p7 = at(x - 1, y + 1);
                uint8_t const p8 = at(x - 1, y);
                uint8_t const p9 = at(x - 1, y - 1);

                int const B = p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9;
                if (B < 2 || B > 6) continue;

                int A = 0;
                if (p2 == 0 && p3 == 1) ++A;
                if (p3 == 0 && p4 == 1) ++A;
                if (p4 == 0 && p5 == 1) ++A;
                if (p5 == 0 && p6 == 1) ++A;
                if (p6 == 0 && p7 == 1) ++A;
                if (p7 == 0 && p8 == 1) ++A;
                if (p8 == 0 && p9 == 1) ++A;
                if (p9 == 0 && p2 == 1) ++A;
                if (A != 1) continue;

                if (p2 * p4 * p8 != 0) continue;
                if (p2 * p6 * p8 != 0) continue;

                to_remove.push_back(y * w + x);
            }
        }
        for (int const idx: to_remove) {
            grid[idx] = 0;
            changed = true;
        }
    }
}

/**
 * @brief Find an endpoint of the skeleton (pixel with exactly 1 neighbor).
 *
 * If no endpoint exists (closed loop), returns any skeleton pixel.
 * Returns {-1, -1} if skeleton is empty.
 */
std::pair<int, int> find_endpoint(std::vector<uint8_t> const & grid, int const w, int const h) {
    std::pair<int, int> any_pixel{-1, -1};

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (grid[y * w + x] == 0) continue;

            if (any_pixel.first < 0) {
                any_pixel = {x, y};
            }

            int neighbors = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    int const nx = x + dx;
                    int const ny = y + dy;
                    if (nx >= 0 && nx < w && ny >= 0 && ny < h && grid[ny * w + nx] != 0) {
                        ++neighbors;
                    }
                }
            }
            if (neighbors == 1) {
                return {x, y};
            }
        }
    }

    return any_pixel;
}

/**
 * @brief Trace the skeleton by following connected pixels from a starting point.
 *
 * Marks visited pixels to avoid revisiting.
 */
std::vector<std::pair<int, int>> trace_skeleton(std::vector<uint8_t> & grid,
                                                int const w, int const h,
                                                std::pair<int, int> const start) {
    std::vector<std::pair<int, int>> path;
    if (start.first < 0) return path;

    int cx = start.first;
    int cy = start.second;
    grid[cy * w + cx] = 0;
    path.emplace_back(cx, cy);

    bool found = true;
    while (found) {
        found = false;
        static constexpr int dx_order[] = {0, 1, 0, -1, 1, 1, -1, -1};
        static constexpr int dy_order[] = {-1, 0, 1, 0, -1, 1, 1, -1};

        for (int i = 0; i < 8; ++i) {
            int const nx = cx + dx_order[i];
            int const ny = cy + dy_order[i];
            if (nx >= 0 && nx < w && ny >= 0 && ny < h && grid[ny * w + nx] != 0) {
                grid[ny * w + nx] = 0;
                path.emplace_back(nx, ny);
                cx = nx;
                cy = ny;
                found = true;
                break;
            }
        }
    }

    return path;
}

/**
 * @brief Scale a point from tensor coordinates to target image coordinates.
 */
Point2D<float> scale_to_target(float const x, float const y,
                               int const tensor_h, int const tensor_w,
                               ImageSize const target) {
    if (target.width <= 0 || target.height <= 0) {
        return {x, y};
    }
    float const sx = static_cast<float>(target.width) / static_cast<float>(tensor_w);
    float const sy = static_cast<float>(target.height) / static_cast<float>(tensor_h);
    return {x * sx, y * sy};
}

}// namespace

std::string TensorToLine2D::name() const {
    return "TensorToLine2D";
}

std::string TensorToLine2D::outputTypeName() const {
    return "Line2D";
}

Line2D TensorToLine2D::decode(at::Tensor const & tensor,
                              DecoderContext const & ctx,
                              LineDecoderParams const & params) {
    validateTensorToLine2DInput(tensor, ctx);

    auto channel = tensor[ctx.batch_index][ctx.source_channel]
                           .to(torch::kCPU)
                           .to(torch::kFloat32)
                           .contiguous();
    auto const h = ctx.height;
    auto const w = ctx.width;
    auto accessor = channel.accessor<float, 2>();

    std::vector<uint8_t> grid(static_cast<size_t>(h * w), 0);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (accessor[y][x] > params.threshold) {
                grid[y * w + x] = 1;
            }
        }
    }

    zhang_suen_thinning(grid, w, h);

    auto const start = find_endpoint(grid, w, h);
    auto const path = trace_skeleton(grid, w, h, start);

    if (path.empty()) {
        return Line2D{};
    }

    std::vector<Point2D<float>> points;
    points.reserve(path.size());
    for (auto const & p: path) {
        points.push_back(scale_to_target(
                static_cast<float>(p.first), static_cast<float>(p.second),
                h, w, ctx.target_image_size));
    }

    return Line2D{std::move(points)};
}

}// namespace dl
