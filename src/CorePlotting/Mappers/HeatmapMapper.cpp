#include "HeatmapMapper.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace CorePlotting {

// =============================================================================
// Auto range
// =============================================================================

std::pair<float, float> HeatmapMapper::computeAutoRange(
    std::vector<HeatmapRowData> const & rows)
{
    double max_val = 0.0;
    for (auto const & row : rows) {
        for (double const v : row.values) {
            max_val = std::max(max_val, v);
        }
    }
    if (max_val <= 0.0) {
        return {0.0f, 1.0f};
    }
    return {0.0f, static_cast<float>(max_val)};
}

// =============================================================================
// Rectangle batch
// =============================================================================

RenderableRectangleBatch HeatmapMapper::toRectangleBatch(
    std::vector<HeatmapRowData> const & rows,
    Colormaps::ColormapFunction const & colormap,
    HeatmapColorRange const & color_range)
{
    RenderableRectangleBatch batch;

    if (rows.empty()) {
        return batch;
    }

    // Determine value range for colormap normalisation
    float vmin = 0.0f;
    float vmax = 1.0f;

    switch (color_range.mode) {
        case HeatmapColorRange::Mode::Auto: {
            auto [auto_min, auto_max] = computeAutoRange(rows);
            vmin = auto_min;
            vmax = auto_max;
            break;
        }
        case HeatmapColorRange::Mode::Manual:
            vmin = color_range.vmin;
            vmax = color_range.vmax;
            break;
        case HeatmapColorRange::Mode::Symmetric: {
            float abs_max = 0.0f;
            for (auto const & row : rows) {
                for (double const v : row.values) {
                    abs_max = std::max(abs_max, static_cast<float>(std::abs(v)));
                }
            }
            if (abs_max <= 0.0f) abs_max = 1.0f;
            vmin = -abs_max;
            vmax = abs_max;
            break;
        }
    }

    // Count total cells for reservation
    std::size_t total_cells = 0;
    for (auto const & row : rows) {
        total_cells += row.values.size();
    }

    batch.bounds.reserve(total_cells);
    batch.colors.reserve(total_cells);

    // Build rectangles: row 0 = first row at bottom (Y=0..1),
    // row 1 = second row (Y=1..2), etc.
    for (std::size_t row_idx = 0; row_idx < rows.size(); ++row_idx) {
        auto const & row = rows[row_idx];
        auto const y = static_cast<float>(row_idx);
        auto const bin_width = static_cast<float>(row.bin_width);

        for (std::size_t col = 0; col < row.values.size(); ++col) {
            auto const x = static_cast<float>(
                row.bin_start + static_cast<double>(col) * row.bin_width);
            auto const value = static_cast<float>(row.values[col]);

            // bounds = (x, y, width, height) where (x,y) is bottom-left
            batch.bounds.emplace_back(x, y, bin_width, 1.0f);

            // Map value through colormap
            batch.colors.push_back(
                Colormaps::mapValue(colormap, value, vmin, vmax));
        }
    }

    return batch;
}

// =============================================================================
// Scene builder
// =============================================================================

RenderableScene HeatmapMapper::buildScene(
    std::vector<HeatmapRowData> const & rows,
    Colormaps::ColormapFunction const & colormap,
    HeatmapColorRange const & color_range)
{
    RenderableScene scene;

    scene.rectangle_batches.push_back(
        toRectangleBatch(rows, colormap, color_range));

    // Identity matrices — the widget overrides with its own projection
    scene.view_matrix = glm::mat4{1.0f};
    scene.projection_matrix = glm::mat4{1.0f};

    return scene;
}

} // namespace CorePlotting
