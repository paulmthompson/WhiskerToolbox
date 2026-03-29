/**
 * @file ApplyFeatureColors.hpp
 * @brief Apply resolved float values as colors to a RenderableScene's glyph batches
 *
 * Takes the per-point optional<float> values from resolveFeatureColors() and a
 * FeatureColorMapping, then overwrites glyph batch colors in the scene.
 * Points with nullopt keep their existing color.
 */

#ifndef COREPLOTTING_FEATURECOLOR_APPLYFEATURECOLORS_HPP
#define COREPLOTTING_FEATURECOLOR_APPLYFEATURECOLORS_HPP

#include "FeatureColorMapping.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <unordered_set>

namespace CorePlotting {
struct RenderableScene;
}// namespace CorePlotting

namespace CorePlotting::FeatureColor {

/**
 * @brief Apply feature coloring to all glyph batches in the scene
 *
 * For each glyph whose entity_id maps to a point index within the resolved values,
 * sets the glyph color according to the mapping. Points in @p skip_indices or with
 * nullopt resolved values retain their existing color.
 *
 * @param scene            Scene whose glyph_batches will be mutated
 * @param resolved_values  Per-point float values (parallel to the point array)
 * @param mapping          How to convert floats to colors
 * @param point_count      Total number of points in the scatter data source
 *                         (used to filter entity_ids belonging to this source)
 * @param skip_indices     Point indices to skip (e.g., selected / navigated points)
 */
void applyFeatureColorsToScene(
        RenderableScene & scene,
        std::span<std::optional<float> const> resolved_values,
        FeatureColorMapping const & mapping,
        std::size_t point_count,
        std::unordered_set<std::size_t> const & skip_indices = {});

}// namespace CorePlotting::FeatureColor

#endif// COREPLOTTING_FEATURECOLOR_APPLYFEATURECOLORS_HPP
