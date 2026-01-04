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

}// namespace CorePlotting