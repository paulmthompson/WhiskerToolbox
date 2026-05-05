/**
 * @file MinMaxPolylineDecimation.cpp
 * @brief Min–max decimation implementation for `RenderablePolyLineBatch`.
 */

#include "CorePlotting/LineDecimation/MinMaxPolylineDecimation.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <vector>

namespace CorePlotting {

namespace {

constexpr float kXSpanEps = 1e-12f;
constexpr float kDedupeEps = 1e-7f;

void appendPairDedupe(std::vector<float> & out, float x, float y) {
    if (out.size() >= 2) {
        float const px = out[out.size() - 2U];
        float const py = out[out.size() - 1U];
        if (std::abs(px - x) <= kDedupeEps && std::abs(py - y) <= kDedupeEps) {
            return;
        }
    }
    out.push_back(x);
    out.push_back(y);
}

struct BucketAcc {
    bool any{false};
    float min_y_val{std::numeric_limits<float>::infinity()};
    float max_y_val{-std::numeric_limits<float>::infinity()};
    float min_y_pt_x{0.0f};
    float min_y_pt_y{0.0f};
    float max_y_pt_x{0.0f};
    float max_y_pt_y{0.0f};

    void consider(float x, float y) {
        any = true;
        if (y < min_y_val || (y == min_y_val && x < min_y_pt_x)) {
            min_y_val = y;
            min_y_pt_x = x;
            min_y_pt_y = y;
        }
        if (y > max_y_val || (y == max_y_val && x < max_y_pt_x)) {
            max_y_val = y;
            max_y_pt_x = x;
            max_y_pt_y = y;
        }
    }
};

void emitBucket(std::vector<float> & merged, BucketAcc const & acc) {
    if (!acc.any) {
        return;
    }
    if (acc.min_y_pt_x == acc.max_y_pt_x && acc.min_y_pt_y == acc.max_y_pt_y) {
        appendPairDedupe(merged, acc.min_y_pt_x, acc.min_y_pt_y);
        return;
    }
    float const x1 = acc.min_y_pt_x;
    float const y1 = acc.min_y_pt_y;
    float const x2 = acc.max_y_pt_x;
    float const y2 = acc.max_y_pt_y;
    if (x1 < x2 || (x1 == x2 && y1 <= y2)) {
        appendPairDedupe(merged, x1, y1);
        appendPairDedupe(merged, x2, y2);
    } else {
        appendPairDedupe(merged, x2, y2);
        appendPairDedupe(merged, x1, y1);
    }
}

[[nodiscard]] std::vector<float> decimateOneStrip(
        std::span<float const> const xy,
        int const vertex_count,
        int const bucket_count) {
    if (vertex_count < 2 || bucket_count <= 0) {
        return {xy.begin(), xy.begin() + static_cast<size_t>(vertex_count) * 2U};
    }

    if (vertex_count <= 2 * bucket_count + 2) {
        return {xy.begin(), xy.begin() + static_cast<size_t>(vertex_count) * 2U};
    }

    int const B = bucket_count;
    std::vector<BucketAcc> buckets(static_cast<size_t>(B));

    float const x_first = xy[0];
    float const x_last = xy[static_cast<size_t>(vertex_count - 1) * 2U];
    float const span = x_last - x_first;
    bool const use_index =
            std::abs(span) < kXSpanEps * (1.0f + std::max(std::abs(x_first), std::abs(x_last)));

    auto const bucket_for = [&](int const idx, float const x) -> int {
        if (use_index) {
            long long const numer = static_cast<long long>(idx) * static_cast<long long>(B);
            int b = static_cast<int>(numer / static_cast<long long>(vertex_count));
            if (b >= B) {
                b = B - 1;
            }
            return b;
        }
        float const inv = static_cast<float>(B) / span;
        float const t = (x - x_first) * inv;
        int b = static_cast<int>(std::floor(static_cast<double>(t)));
        if (b < 0) {
            b = 0;
        }
        if (b >= B) {
            b = B - 1;
        }
        return b;
    };

    for (int i = 0; i < vertex_count; ++i) {
        size_t const o = static_cast<size_t>(i) * 2U;
        float const x = xy[o];
        float const y = xy[o + 1U];
        int const b = bucket_for(i, x);
        buckets[static_cast<size_t>(b)].consider(x, y);
    }

    float const fx = xy[0];
    float const fy = xy[1];
    size_t const last_o = static_cast<size_t>(vertex_count - 1) * 2U;
    float const lx = xy[last_o];
    float const ly = xy[last_o + 1U];

    std::vector<float> merged;
    merged.reserve(static_cast<size_t>(B) * 4U + 4U);
    appendPairDedupe(merged, fx, fy);
    for (int b = 0; b < B; ++b) {
        emitBucket(merged, buckets[static_cast<size_t>(b)]);
    }
    appendPairDedupe(merged, lx, ly);

    if (merged.size() < 4U) {
        return {xy.begin(), xy.begin() + static_cast<size_t>(vertex_count) * 2U};
    }
    return merged;
}

}// namespace

RenderablePolyLineBatch decimatePolyLineBatchMinMax(
        RenderablePolyLineBatch const & in,
        MinMaxDecimationParams const params) {
    if (params.bucket_count <= 0) {
        return in;
    }

    if (in.line_start_indices.size() != in.line_vertex_counts.size()) {
        return in;
    }

    if (in.vertices.size() < 4U || in.line_start_indices.empty()) {
        return in;
    }

    RenderablePolyLineBatch out;
    out.global_color = in.global_color;
    out.global_entity_id = in.global_entity_id;
    out.thickness = in.thickness;
    out.model_matrix = in.model_matrix;
    out.entity_ids = in.entity_ids;
    out.colors = in.colors;

    int32_t next_start = 0;
    for (size_t li = 0; li < in.line_start_indices.size(); ++li) {
        int32_t const start_v = in.line_start_indices[li];
        int32_t const nv = in.line_vertex_counts[li];
        size_t const float_begin = static_cast<size_t>(start_v) * 2U;
        size_t const float_count = static_cast<size_t>(nv) * 2U;
        if (float_begin + float_count > in.vertices.size()) {
            return in;
        }
        if (nv < 2) {
            out.line_start_indices.push_back(next_start);
            out.line_vertex_counts.push_back(nv);
            next_start += nv;
            out.vertices.insert(
                    out.vertices.end(),
                    in.vertices.begin() + static_cast<std::vector<float>::difference_type>(float_begin),
                    in.vertices.begin() + static_cast<std::vector<float>::difference_type>(float_begin + float_count));
            continue;
        }
        std::span<float const> const strip{
                in.vertices.data() + float_begin,
                static_cast<size_t>(nv) * 2U};
        std::vector<float> line_verts = decimateOneStrip(strip, nv, params.bucket_count);
        if (line_verts.size() < 4U) {
            continue;
        }
        out.line_start_indices.push_back(next_start);
        out.line_vertex_counts.push_back(static_cast<int32_t>(line_verts.size() / 2U));
        next_start += static_cast<int32_t>(line_verts.size() / 2U);
        out.vertices.insert(out.vertices.end(), line_verts.begin(), line_verts.end());
    }

    if (out.vertices.empty() || out.line_start_indices.empty()) {
        return in;
    }
    return out;
}

}// namespace CorePlotting
