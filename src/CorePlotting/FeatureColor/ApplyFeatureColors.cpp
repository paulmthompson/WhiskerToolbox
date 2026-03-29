/**
 * @file ApplyFeatureColors.cpp
 * @brief Implementation of per-point feature color application to glyph batches
 */

#include "ApplyFeatureColors.hpp"

#include "CorePlotting/Colormaps/Colormap.hpp"
#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"

#include <cassert>
#include <variant>

namespace CorePlotting::FeatureColor {

namespace {

/**
 * @brief Map a single float value to a color via ContinuousMapping
 */
glm::vec4 mapContinuous(ContinuousMapping const & mapping, float value) {
    auto const cmap = Colormaps::getColormap(mapping.preset);
    return Colormaps::mapValue(cmap, value, mapping.vmin, mapping.vmax);
}

/**
 * @brief Map a single float value to a color via ThresholdMapping
 */
glm::vec4 mapThreshold(ThresholdMapping const & mapping, float value) {
    return value >= mapping.threshold ? mapping.above_color : mapping.below_color;
}

/**
 * @brief Map a float value through the variant mapping
 */
glm::vec4 mapValue(FeatureColorMapping const & mapping, float value) {
    return std::visit(
            [value](auto const & m) -> glm::vec4 {
                using T = std::decay_t<decltype(m)>;
                if constexpr (std::is_same_v<T, ContinuousMapping>) {
                    return mapContinuous(m, value);
                } else {
                    return mapThreshold(m, value);
                }
            },
            mapping);
}

}// anonymous namespace

void applyFeatureColorsToScene(
        RenderableScene & scene,
        std::span<std::optional<float> const> resolved_values,
        FeatureColorMapping const & mapping,
        std::size_t point_count,
        std::unordered_set<std::size_t> const & skip_indices) {

    if (resolved_values.empty()) {
        return;
    }

    for (auto & batch: scene.glyph_batches) {
        for (std::size_t i = 0; i < batch.entity_ids.size(); ++i) {
            auto const scatter_idx = static_cast<std::size_t>(batch.entity_ids[i].id);

            // Only process glyphs that belong to the scatter point array
            if (scatter_idx >= point_count) {
                continue;
            }

            // Skip selected / navigated points — they keep their dedicated colors
            if (skip_indices.contains(scatter_idx)) {
                continue;
            }

            // Skip points without a resolved value
            if (scatter_idx >= resolved_values.size() || !resolved_values[scatter_idx].has_value()) {
                continue;
            }

            float const val = resolved_values[scatter_idx].value(); // NOLINT(bugprone-unchecked-optional-access)
            batch.colors[i] = mapValue(mapping, val);
        }
    }
}

}// namespace CorePlotting::FeatureColor
