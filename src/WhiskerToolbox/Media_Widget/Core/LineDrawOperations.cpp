/**
 * @file LineDrawOperations.cpp
 * @brief Qt-free helpers for committing line geometry from Media Viewer pen tools
 */

#include "LineDrawOperations.hpp"

#include "Core/MediaWidgetState.hpp"
#include "DisplayOptions/DisplayOptions.hpp"
#include "Rendering/Media_Window/Media_Window.hpp"

#include "CorePlotting/Layout/CanvasCoordinateSystem.hpp"
#include "Lines/Line_Data.hpp"

#include <QString>

Point2D<float> mediaCoordsToLineDataCoords(float x_media,
                                           float y_media,
                                           Media_Window const * scene,
                                           MediaWidgetState const * state,
                                           LineData const & line_data,
                                           std::string const & line_key) {
    if (scene == nullptr) {
        return {x_media, y_media};
    }

    float const scene_x = x_media * scene->getXAspect();
    float const scene_y = y_media * scene->getYAspect();

    CoordinateMappingMode mapping = CoordinateMappingMode::ScaleToCanvas;
    if (state != nullptr) {
        if (auto const * config = state->displayOptions().get<LineDisplayOptions>(
                    QString::fromStdString(line_key))) {
            mapping = config->coordinate_mapping;
        }
    }

    auto const [canvas_width, canvas_height] = scene->getCanvasSize();
    auto const factors = computeScalingFactors(
            canvas_width,
            canvas_height,
            scene->canvasCoordinateSystem(),
            line_data.getImageSize(),
            mapping);

    return {scene_x / factors.x, scene_y / factors.y};
}

std::optional<EntityId> commitNewLineAtTime(LineData & line_data,
                                            TimeFrameIndex time,
                                            Line2D const & line,
                                            NotifyObservers notify) {
    EntityId const entity_id = line_data.addAtTime(time, line, notify);
    if (entity_id == EntityId(0)) {
        return std::nullopt;
    }
    return entity_id;
}

bool entityExistsAtTime(LineData const & line_data, EntityId entity_id, TimeFrameIndex time) {
    auto const entity_time = line_data.getTimeByEntityId(entity_id);
    return entity_time.has_value() && entity_time.value() == time;
}
