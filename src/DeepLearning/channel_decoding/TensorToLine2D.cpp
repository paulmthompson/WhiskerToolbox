#include "TensorToLine2D.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace dl {

std::string TensorToLine2D::name() const
{
    return "TensorToLine2D";
}

std::string TensorToLine2D::outputTypeName() const
{
    return "Line2D";
}

namespace {

/// Zhang-Suen thinning algorithm.
/// Thins a binary image in-place to a 1-pixel-wide skeleton.
/// grid is row-major: grid[y * w + x], with values 0 or 1.
void zhang_suen_thinning(std::vector<uint8_t> & grid, int const w, int const h)
{
    bool changed = true;

    auto at = [&](int x, int y) -> uint8_t {
        if (x < 0 || x >= w || y < 0 || y >= h) return 0;
        return grid[y * w + x];
    };

    while (changed) {
        changed = false;

        // Sub-iteration 1
        std::vector<int> to_remove;
        for (int y = 1; y < h - 1; ++y) {
            for (int x = 1; x < w - 1; ++x) {
                if (grid[y * w + x] == 0) continue;

                // 8-neighbors in order: P2(N), P3(NE), P4(E), P5(SE),
                //                       P6(S), P7(SW), P8(W), P9(NW)
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

                // Count 0→1 transitions in the ordered sequence
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

                // Sub-iter 1 conditions
                if (p2 * p4 * p6 != 0) continue;
                if (p4 * p6 * p8 != 0) continue;

                to_remove.push_back(y * w + x);
            }
        }
        for (int const idx : to_remove) {
            grid[idx] = 0;
            changed = true;
        }

        // Sub-iteration 2
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

                // Sub-iter 2 conditions (different from sub-iter 1)
                if (p2 * p4 * p8 != 0) continue;
                if (p2 * p6 * p8 != 0) continue;

                to_remove.push_back(y * w + x);
            }
        }
        for (int const idx : to_remove) {
            grid[idx] = 0;
            changed = true;
        }
    }
}

/// Find an endpoint of the skeleton (pixel with exactly 1 neighbor).
/// If no endpoint exists (closed loop), return any skeleton pixel.
/// Returns {-1, -1} if skeleton is empty.
std::pair<int, int> find_endpoint(std::vector<uint8_t> const & grid, int const w, int const h)
{
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

    return any_pixel; // fallback (loop or single pixel)
}

/// Trace the skeleton by following connected pixels from a starting point.
/// Marks visited pixels to avoid revisiting.
std::vector<std::pair<int, int>> trace_skeleton(std::vector<uint8_t> & grid,
                                                int const w, int const h,
                                                std::pair<int, int> const start)
{
    std::vector<std::pair<int, int>> path;
    if (start.first < 0) return path;

    int cx = start.first;
    int cy = start.second;
    grid[cy * w + cx] = 0; // mark visited
    path.push_back({cx, cy});

    bool found = true;
    while (found) {
        found = false;
        // Prefer 4-connected neighbors first (N, E, S, W), then diagonals
        // to produce smoother lines
        static constexpr int dx_order[] = {0, 1, 0, -1, 1, 1, -1, -1};
        static constexpr int dy_order[] = {-1, 0, 1, 0, -1, 1, 1, -1};

        for (int i = 0; i < 8; ++i) {
            int const nx = cx + dx_order[i];
            int const ny = cy + dy_order[i];
            if (nx >= 0 && nx < w && ny >= 0 && ny < h && grid[ny * w + nx] != 0) {
                grid[ny * w + nx] = 0; // mark visited
                path.push_back({nx, ny});
                cx = nx;
                cy = ny;
                found = true;
                break;
            }
        }
    }

    return path;
}

/// Scale a point from tensor coordinates to target image coordinates
Point2D<float> scale_to_target(float const x, float const y,
                               int const tensor_h, int const tensor_w,
                               ImageSize const target)
{
    if (target.width <= 0 || target.height <= 0) {
        return {x, y};
    }
    float const sx = static_cast<float>(target.width) / static_cast<float>(tensor_w);
    float const sy = static_cast<float>(target.height) / static_cast<float>(tensor_h);
    return {x * sx, y * sy};
}

} // anonymous namespace

Line2D TensorToLine2D::decode(torch::Tensor const & tensor,
                              DecoderParams const & params) const
{
    auto channel = tensor[params.batch_index][params.source_channel];
    auto const h = params.height;
    auto const w = params.width;
    auto accessor = channel.accessor<float, 2>();

    // Step 1: Threshold to binary grid
    std::vector<uint8_t> grid(static_cast<size_t>(h * w), 0);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (accessor[y][x] > params.threshold) {
                grid[y * w + x] = 1;
            }
        }
    }

    // Step 2: Skeletonize
    zhang_suen_thinning(grid, w, h);

    // Step 3: Find an endpoint and trace
    auto const start = find_endpoint(grid, w, h);
    auto const path = trace_skeleton(grid, w, h, start);

    if (path.empty()) {
        return Line2D{};
    }

    // Step 4: Convert to Line2D with coordinate scaling
    std::vector<Point2D<float>> points;
    points.reserve(path.size());
    for (auto const & p : path) {
        points.push_back(scale_to_target(
            static_cast<float>(p.first), static_cast<float>(p.second),
            h, w, params.target_image_size));
    }

    return Line2D{std::move(points)};
}

} // namespace dl
