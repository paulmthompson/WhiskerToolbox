#ifndef DATAVIEWER_TRANSFORMCOMPOSERS_HPP
#define DATAVIEWER_TRANSFORMCOMPOSERS_HPP

#include "CorePlotting/Layout/LayoutTransform.hpp"
#include "CorePlotting/Layout/NormalizationHelpers.hpp"
#include "CorePlotting/Layout/SeriesLayout.hpp"

namespace DataViewer {

/**
 * @brief Compose Y transform for analog series rendering
 * 
 * Follows the pattern from CorePlotting/DESIGN.md:
 * 1. Data normalization (z-score style: maps ±3σ to ±1)
 * 2. User adjustments (intrinsic scale, user scale, vertical offset)
 * 3. Layout positioning (from LayoutEngine)
 * 4. Global scaling (from ViewState) - applied to amplitude only, NOT position
 * 
 * IMPORTANT: Global zoom scales the data amplitude within each lane, but does NOT
 * move the lane center. This is achieved by applying global scaling to the gain
 * component only, after composing data normalization with layout positioning.
 */
[[nodiscard]] inline CorePlotting::LayoutTransform composeAnalogYTransform(
        CorePlotting::SeriesLayout const & layout,
        float data_mean,
        float std_dev,
        float intrinsic_scale,
        float user_scale_factor,
        float user_vertical_offset,
        float global_zoom,
        float global_vertical_scale) {

    // Use NormalizationHelpers to create the data normalization transform
    // forStdDevRange maps mean ± 3*std_dev to ±1
    auto data_norm = CorePlotting::NormalizationHelpers::forStdDevRange(data_mean, std_dev, 3.0f);

    // User adjustments: additional scaling and offset
    auto user_adj = CorePlotting::NormalizationHelpers::manual(
            intrinsic_scale * user_scale_factor,
            user_vertical_offset);

    // Compose data normalization with user adjustments
    // This gives us normalized data in [-1, 1] range (assuming ±3σ coverage)
    auto data_transform = user_adj.compose(data_norm);

    // Layout provides: offset = lane center, gain = half_height of lane
    // Apply 80% margin factor within allocated space
    constexpr float margin_factor = 0.8f;

    // Global scaling affects the amplitude within the lane, NOT the lane position
    // So we apply global_zoom to the gain only
    float const lane_half_height = layout.y_transform.gain * margin_factor;
    float const effective_gain = lane_half_height * global_zoom * global_vertical_scale;

    // Final transform:
    // 1. Apply data_transform to normalize the raw data
    // 2. Scale by effective_gain (includes layout height + global zoom)
    // 3. Translate to lane center (layout.offset is NOT scaled by global_zoom)
    float const final_gain = data_transform.gain * effective_gain;
    float const final_offset = data_transform.offset * effective_gain + layout.y_transform.offset;

    return CorePlotting::LayoutTransform{final_offset, final_gain};
}

/**
 * @brief Compose Y transform for event series (stacked mode)
 */
[[nodiscard]] inline CorePlotting::LayoutTransform composeEventYTransform(
        CorePlotting::SeriesLayout const & layout,
        float margin_factor,
        float global_vertical_scale) {

    // Events map [-1, 1] to allocated space with margin
    // Note: layout.y_transform.gain already represents half-height (maps [-1,1] to allocated space)
    float const half_height = layout.y_transform.gain * margin_factor * global_vertical_scale;
    float const center = layout.y_transform.offset;

    return CorePlotting::LayoutTransform{center, half_height};
}

/**
 * @brief Compose Y transform for event series (full canvas mode)
 */
[[nodiscard]] inline CorePlotting::LayoutTransform composeEventFullCanvasYTransform(
        float viewport_y_min,
        float viewport_y_max,
        float margin_factor) {

    // Full canvas: map [-1, 1] to viewport bounds with margin
    float const height = (viewport_y_max - viewport_y_min) * margin_factor;
    float const center = (viewport_y_max + viewport_y_min) * 0.5f;
    float const half_height = height * 0.5f;

    return CorePlotting::LayoutTransform{center, half_height};
}

/**
 * @brief Compose Y transform for interval series
 * 
 * Note: Intervals intentionally ignore global_zoom because:
 * 1. They are already in normalized space [-1, 1] representing full height
 * 2. global_zoom is designed for scaling analog data based on std_dev
 * 3. Intervals should always fill their allocated canvas space
 */
[[nodiscard]] inline CorePlotting::LayoutTransform composeIntervalYTransform(
        CorePlotting::SeriesLayout const & layout,
        float margin_factor,
        [[maybe_unused]] float global_zoom,
        [[maybe_unused]] float global_vertical_scale) {

    // Intervals map [-1, 1] to allocated space with margin only
    // Note: layout.y_transform.gain already represents half-height (maps [-1,1] to allocated space)
    // We do NOT apply global_zoom here - intervals should fill their allocated space
    float const half_height = layout.y_transform.gain * margin_factor;
    float const center = layout.y_transform.offset;

    return CorePlotting::LayoutTransform{center, half_height};
}

} // namespace DataViewer

#endif // DATAVIEWER_TRANSFORMCOMPOSERS_HPP
