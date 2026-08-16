#include "MediaPoint_Widget.hpp"
#include "ui_MediaPoint_Widget.h"

#include "Media_Widget/Core/MediaWidgetState.hpp"
#include "Media_Widget/DisplayOptions/DisplayOptions.hpp"
#include "Media_Widget/Rendering/Media_Window/Media_Window.hpp"

#include "CorePlotting/Layout/CanvasCoordinateSystem.hpp"
#include "DataManager/DataManager.hpp"
#include "Points/Point_Data.hpp"

#include "Core/GlyphStyleState.hpp"
#include "GlyphStyleControls.hpp"

#include <QPointF>

#include <spdlog/spdlog.h>

namespace {

/**
 * @brief Convert canvas-coordinate click position to point data coordinates.
 *
 * Uses the same scaling factors as Media_Window rendering and hit-testing.
 */
Point2D<float> mediaCoordsToDataCoords(qreal x_media,
                                       qreal y_media,
                                       Media_Window const * scene,
                                       MediaWidgetState const * state,
                                       PointData const & point_data,
                                       std::string const & active_key) {
    float const scene_x = static_cast<float>(x_media) * scene->getXAspect();
    float const scene_y = static_cast<float>(y_media) * scene->getYAspect();

    CoordinateMappingMode mapping = CoordinateMappingMode::ScaleToCanvas;
    if (state != nullptr) {
        if (auto const * config = state->displayOptions().get<PointDisplayOptions>(
                    QString::fromStdString(active_key))) {
            mapping = config->coordinate_mapping;
        }
    }

    auto const [canvas_width, canvas_height] = scene->getCanvasSize();
    auto const factors = computeScalingFactors(
            canvas_width,
            canvas_height,
            scene->canvasCoordinateSystem(),
            point_data.getImageSize(),
            mapping);

    return {scene_x / factors.x, scene_y / factors.y};
}

}// namespace

MediaPoint_Widget::MediaPoint_Widget(std::shared_ptr<DataManager> data_manager, Media_Window * scene, MediaWidgetState * state, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::MediaPoint_Widget),
      _data_manager{std::move(data_manager)},
      _scene{scene},
      _state{state} {
    ui->setupUi(this);

    // Create GlyphStyleState and GlyphStyleControls
    _glyph_state = new GlyphStyleState(this);
    _glyph_controls = new GlyphStyleControls(_glyph_state, this);

    // Insert GlyphStyleControls into the layout after the header grid
    ui->verticalLayout->insertWidget(1, _glyph_controls);

    // Connect GlyphStyleState changes to update PointDisplayOptions
    connect(_glyph_state, &GlyphStyleState::styleChanged,
            this, &MediaPoint_Widget::_applyGlyphStateToOptions);
}

MediaPoint_Widget::~MediaPoint_Widget() {
    delete ui;
}

void MediaPoint_Widget::showEvent(QShowEvent * event) {

    static_cast<void>(event);

    spdlog::debug("MediaPoint_Widget: showEvent");
    // Connect to the new signal that provides modifier information
    connect(_scene, &Media_Window::leftClickMediaWithEvent, this, &MediaPoint_Widget::_handlePointClickWithModifiers);
}

void MediaPoint_Widget::hideEvent(QHideEvent * event) {

    static_cast<void>(event);

    spdlog::debug("MediaPoint_Widget: hideEvent");

    // Guard against _scene being destroyed before hideEvent is called
    if (!_scene) {
        return;
    }

    disconnect(_scene, &Media_Window::leftClickMediaWithEvent, this, &MediaPoint_Widget::_handlePointClickWithModifiers);
    _clearPointSelection();// Clear selection when widget is hidden
}

void MediaPoint_Widget::setActiveKey(std::string const & key) {
    _active_key = key;
    ui->name_label->setText(QString::fromStdString(key));
    _selection_enabled = !key.empty();

    // Sync GlyphStyleState from the current PointDisplayOptions
    _syncGlyphStateFromOptions();
}


void MediaPoint_Widget::_handlePointClickWithModifiers(qreal x_media, qreal y_media, Qt::KeyboardModifiers modifiers) {
    if (!_selection_enabled || _active_key.empty())
        return;

    // Check if Alt is held for point creation
    if (modifiers & Qt::AltModifier) {
        // Alt+click: add new point at current time
        _addPointAtCurrentTime(x_media, y_media);
        return;
    }

    // Check if Ctrl is held for point movement
    if (modifiers & Qt::ControlModifier) {
        EntityId const move_target = _resolveMoveTargetId();
        if (move_target != EntityId(0)) {
            _movePoint(move_target, x_media, y_media);
        }
        return;
    }

    _selectPointAtClick(x_media, y_media);
}

void MediaPoint_Widget::_selectPointAtClick(qreal x_media, qreal y_media) {
    QPointF const scene_pos(x_media * _scene->getXAspect(), y_media * _scene->getYAspect());
    EntityId const entity_id = _scene->findPointAtPosition(scene_pos, _active_key);

    if (entity_id != EntityId(0)) {
        _selectPoint(entity_id);
    } else {
        _clearPointSelection();
    }
}

void MediaPoint_Widget::_selectPoint(EntityId point_id) {
    _selected_point_id = point_id;

    // Use Media_Window's selection system for visual feedback
    _scene->selectEntity(point_id, _active_key, "point");
    spdlog::debug("MediaPoint_Widget: selected point EntityID {}", point_id.id);
}

void MediaPoint_Widget::_clearPointSelection() {
    if (_selected_point_id != EntityId(0)) {
        _selected_point_id = EntityId(0);
        _scene->clearAllSelections();// Clear all selections in scene
        _scene->UpdateCanvas();      // Refresh to remove selection highlight
        spdlog::debug("MediaPoint_Widget: cleared point selection");
    }
}

EntityId MediaPoint_Widget::_resolveMoveTargetId() {
    if (_selected_point_id != EntityId(0)) {
        return _selected_point_id;
    }

    auto const selected_entities = _scene->getSelectedEntities();
    if (selected_entities.empty() || _active_key.empty()) {
        return EntityId(0);
    }

    auto point_data = _data_manager->getData<PointData>(_active_key);
    if (!point_data) {
        return EntityId(0);
    }

    for (EntityId const entity_id: selected_entities) {
        if (point_data->getMutableData(entity_id, NotifyObservers::No).has_value()) {
            _selected_point_id = entity_id;
            return entity_id;
        }
    }

    return EntityId(0);
}

Point2D<float> MediaPoint_Widget::_mediaCoordsToDataCoords(qreal x_media, qreal y_media) const {
    auto point_data = _data_manager->getData<PointData>(_active_key);
    if (!point_data) {
        return {static_cast<float>(x_media), static_cast<float>(y_media)};
    }

    return mediaCoordsToDataCoords(x_media, y_media, _scene, _state, *point_data, _active_key);
}

void MediaPoint_Widget::_movePoint(EntityId point_id, qreal x_media, qreal y_media) {
    if (point_id == EntityId(0) || _active_key.empty()) {
        return;
    }

    auto point_data = _data_manager->getData<PointData>(_active_key);
    if (!point_data) {
        return;
    }

    // Modify the selected point via EntityId using the PointData modification handle
    auto point_handle_opt = point_data->getMutableData(point_id, NotifyObservers::Yes);
    if (!point_handle_opt.has_value()) {
        spdlog::debug("MediaPoint_Widget: could not get mutable point for EntityID {}", point_id.id);
        return;
    }

    Point2D<float> const data_coords = _mediaCoordsToDataCoords(x_media, y_media);
    Point2D<float> & point_ref = point_handle_opt.value().get();
    point_ref.x = data_coords.x;
    point_ref.y = data_coords.y;

    _selected_point_id = point_id;
    _scene->UpdateCanvas();
    spdlog::debug("MediaPoint_Widget: moved point EntityID {} to ({}, {})", point_id.id, data_coords.x, data_coords.y);
}

void MediaPoint_Widget::_assignPoint(qreal x_media, qreal y_media) {
    // Legacy method - now just calls the move function for compatibility
    EntityId const move_target = _resolveMoveTargetId();
    if (move_target != EntityId(0)) {
        _movePoint(move_target, x_media, y_media);
    }
}

void MediaPoint_Widget::_addPointAtCurrentTime(qreal x_media, qreal y_media) {
    if (_active_key.empty())
        return;

    auto point_data = _data_manager->getData<PointData>(_active_key);

    auto time_position = _state->current_position;
    auto point_time_index = time_position.convertTo(point_data->getTimeFrame().get());

    if (point_data) {
        Point2D<float> const data_coords = _mediaCoordsToDataCoords(x_media, y_media);
        point_data->addAtTime(point_time_index, data_coords, NotifyObservers::No);
        spdlog::debug("MediaPoint_Widget: added point at ({}, {}) at time {}", data_coords.x, data_coords.y, point_time_index.getValue());
        point_data->notifyObservers();
    }
}

void MediaPoint_Widget::_syncGlyphStateFromOptions() {
    if (_active_key.empty() || !_state || !_glyph_state) {
        return;
    }

    auto const * config = _state->displayOptions().get<PointDisplayOptions>(QString::fromStdString(_active_key));
    if (!config) {
        return;
    }

    CorePlotting::GlyphStyleData style;
    style.glyph_type = config->marker_shape;
    style.size = static_cast<float>(config->point_size);
    style.hex_color = config->hex_color();
    style.alpha = config->alpha();
    _glyph_state->setStyleSilent(style);
}

void MediaPoint_Widget::_applyGlyphStateToOptions() {
    if (_active_key.empty() || !_state || !_glyph_state) {
        return;
    }

    auto const key = QString::fromStdString(_active_key);
    auto * point_opts = _state->displayOptions().getMutable<PointDisplayOptions>(key);
    if (!point_opts) {
        return;
    }

    auto const & style = _glyph_state->data();
    point_opts->marker_shape = style.glyph_type;
    point_opts->point_size = static_cast<int>(style.size);
    point_opts->hex_color() = style.hex_color;
    point_opts->alpha() = style.alpha;
    _state->displayOptions().notifyChanged<PointDisplayOptions>(key);
    _scene->UpdateCanvas();
}
