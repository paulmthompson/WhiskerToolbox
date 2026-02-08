#include "HistogramMapper.hpp"

#include <cstdint>

namespace CorePlotting {

RenderableRectangleBatch HistogramMapper::toBars(
    HistogramData const & data,
    HistogramStyle const & style)
{
    RenderableRectangleBatch batch;

    if (data.counts.empty()) {
        return batch;
    }

    float const gap = static_cast<float>(data.bin_width) * style.bar_gap_fraction;

    batch.bounds.reserve(data.numBins());
    batch.colors.reserve(data.numBins());

    for (std::size_t i = 0; i < data.numBins(); ++i) {
        float const left = static_cast<float>(data.binLeft(i)) + gap;
        float const width = static_cast<float>(data.bin_width) - 2.0f * gap;
        float const height = static_cast<float>(data.counts[i]);

        // Skip bins with zero height (optional — keeps GPU work down)
        if (height <= 0.0f) {
            continue;
        }

        // bounds = (x, y, width, height) where (x,y) is bottom-left
        batch.bounds.emplace_back(left, 0.0f, width, height);
        batch.colors.push_back(style.fill_color);
    }

    return batch;
}

RenderablePolyLineBatch HistogramMapper::toLine(
    HistogramData const & data,
    HistogramStyle const & style)
{
    RenderablePolyLineBatch batch;

    if (data.counts.empty()) {
        return batch;
    }

    batch.thickness = style.line_thickness;
    batch.global_color = style.line_color;

    // Build a step-function polyline:
    //   For each bin: add (left, height) then (right, height)
    //   This produces a classic histogram outline.
    //
    // Start at y=0 on the left edge, step up/down at each bin boundary.
    batch.vertices.reserve((data.numBins() * 4 + 4));

    // Start at (first_bin_left, 0)
    batch.vertices.push_back(static_cast<float>(data.binLeft(0)));
    batch.vertices.push_back(0.0f);

    for (std::size_t i = 0; i < data.numBins(); ++i) {
        float const left  = static_cast<float>(data.binLeft(i));
        float const right = static_cast<float>(data.binRight(i));
        float const h     = static_cast<float>(data.counts[i]);

        // Vertical step up to bin height at left edge
        batch.vertices.push_back(left);
        batch.vertices.push_back(h);

        // Horizontal across the bin
        batch.vertices.push_back(right);
        batch.vertices.push_back(h);
    }

    // Step back down to y=0 at the right edge
    batch.vertices.push_back(static_cast<float>(data.binEnd()));
    batch.vertices.push_back(0.0f);

    // Single line segment spanning all vertices
    batch.line_start_indices.push_back(0);
    batch.line_vertex_counts.push_back(
        static_cast<int32_t>(batch.vertices.size() / 2));

    return batch;
}

RenderableScene HistogramMapper::buildScene(
    HistogramData const & data,
    HistogramDisplayMode mode,
    HistogramStyle const & style)
{
    RenderableScene scene;

    switch (mode) {
        case HistogramDisplayMode::Bar:
            scene.rectangle_batches.push_back(toBars(data, style));
            break;
        case HistogramDisplayMode::Line:
            scene.poly_line_batches.push_back(toLine(data, style));
            break;
    }

    // Identity matrices — the widget overrides with its own projection
    scene.view_matrix = glm::mat4{1.0f};
    scene.projection_matrix = glm::mat4{1.0f};

    return scene;
}

} // namespace CorePlotting
