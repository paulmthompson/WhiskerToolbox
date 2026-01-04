#include "TimeAxisCoordinates.hpp"

namespace CorePlotting {

float canvasXToTime(float canvas_x, TimeAxisParams const & params) {
    if (params.viewport_width_px <= 0) {
        return static_cast<float>(params.time_start);
    }

    float const normalized_x = canvas_x / static_cast<float>(params.viewport_width_px);
    float const time_span = static_cast<float>(params.time_end - params.time_start);

    return static_cast<float>(params.time_start) + normalized_x * time_span;
}

float timeToCanvasX(float time, TimeAxisParams const & params) {
    float const time_span = static_cast<float>(params.time_end - params.time_start);

    if (time_span <= 0.0f) {
        return 0.0f;
    }

    float const normalized_x = (time - static_cast<float>(params.time_start)) / time_span;
    return normalized_x * static_cast<float>(params.viewport_width_px);
}

float timeToNDC(float time, TimeAxisParams const & params) {
    float const time_span = static_cast<float>(params.time_end - params.time_start);

    if (time_span <= 0.0f) {
        return 0.0f;
    }

    // Map [time_start, time_end] to [-1, +1]
    float const normalized = (time - static_cast<float>(params.time_start)) / time_span;
    return 2.0f * normalized - 1.0f;
}

float ndcToTime(float ndc_x, TimeAxisParams const & params) {
    // Map [-1, +1] to [time_start, time_end]
    float const normalized = (ndc_x + 1.0f) / 2.0f;
    float const time_span = static_cast<float>(params.time_end - params.time_start);

    return static_cast<float>(params.time_start) + normalized * time_span;
}

float pixelsPerTimeUnit(TimeAxisParams const & params) {
    float const time_span = static_cast<float>(params.time_end - params.time_start);

    if (time_span <= 0.0f) {
        return 0.0f;
    }

    return static_cast<float>(params.viewport_width_px) / time_span;
}

float timeUnitsPerPixel(TimeAxisParams const & params) {
    if (params.viewport_width_px <= 0) {
        return 0.0f;
    }

    float const time_span = static_cast<float>(params.time_end - params.time_start);
    return time_span / static_cast<float>(params.viewport_width_px);
}

// ============================================================================
// Y-Axis Coordinate Conversions
// ============================================================================

float canvasYToWorldY(float canvas_y, YAxisParams const & params) {
    if (params.viewport_height_px <= 0) {
        return params.world_y_min + params.pan_offset;
    }

    // Canvas Y: 0 = top, viewport_height = bottom
    // World Y: y_max = top, y_min = bottom
    // So we need to invert: normalized_y = 1 - (canvas_y / height)
    float const normalized_y = 1.0f - (canvas_y / static_cast<float>(params.viewport_height_px));

    // Apply pan offset to effective bounds
    float const effective_y_min = params.world_y_min + params.pan_offset;
    float const effective_y_max = params.world_y_max + params.pan_offset;

    // Map [0, 1] to [effective_y_min, effective_y_max]
    return effective_y_min + normalized_y * (effective_y_max - effective_y_min);
}

float worldYToCanvasY(float world_y, YAxisParams const & params) {
    // Apply pan offset to effective bounds
    float const effective_y_min = params.world_y_min + params.pan_offset;
    float const effective_y_max = params.world_y_max + params.pan_offset;
    float const y_range = effective_y_max - effective_y_min;

    if (y_range <= 0.0f || params.viewport_height_px <= 0) {
        return 0.0f;
    }

    // Map world_y to normalized [0, 1] where 0 = bottom, 1 = top
    float const normalized_y = (world_y - effective_y_min) / y_range;

    // Invert for canvas coordinates (0 = top, height = bottom)
    return (1.0f - normalized_y) * static_cast<float>(params.viewport_height_px);
}

float worldYToNDC(float world_y, YAxisParams const & params) {
    float const effective_y_min = params.world_y_min + params.pan_offset;
    float const effective_y_max = params.world_y_max + params.pan_offset;
    float const y_range = effective_y_max - effective_y_min;

    if (y_range <= 0.0f) {
        return 0.0f;
    }

    // Map [effective_y_min, effective_y_max] to [-1, +1]
    float const normalized = (world_y - effective_y_min) / y_range;
    return 2.0f * normalized - 1.0f;
}

float ndcToWorldY(float ndc_y, YAxisParams const & params) {
    float const effective_y_min = params.world_y_min + params.pan_offset;
    float const effective_y_max = params.world_y_max + params.pan_offset;

    // Map [-1, +1] to [effective_y_min, effective_y_max]
    float const normalized = (ndc_y + 1.0f) / 2.0f;
    return effective_y_min + normalized * (effective_y_max - effective_y_min);
}

}// namespace CorePlotting