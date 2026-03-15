#include "MediaPoint_Widget.hpp"
#include "ui_MediaPoint_Widget.h"

#include "Media_Widget/Core/MediaWidgetState.hpp"
#include "Media_Widget/DisplayOptions/DisplayOptions.hpp"
#include "Media_Widget/Rendering/Media_Window/Media_Window.hpp"

#include "DataManager/DataManager.hpp"
#include "Points/Point_Data.hpp"

#include "Core/GlyphStyleState.hpp"
#include "GlyphStyleControls.hpp"

#include <QPointF>
#include <cmath>
#include <iostream>

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

    std::cout << "Show Event" << std::endl;
    // Connect to the new signal that provides modifier information
    connect(_scene, &Media_Window::leftClickMediaWithEvent, this, &MediaPoint_Widget::_handlePointClickWithModifiers);
}

void MediaPoint_Widget::hideEvent(QHideEvent * event) {

    static_cast<void>(event);

    std::cout << "Hide Event" << std::endl;

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
        // Ctrl+click: move selected point if one is selected
        if (_selected_point_id != EntityId(0)) {
            _moveSelectedPoint(x_media, y_media);
        }
        return;
    }

    // Regular click: select nearby point if exists
    EntityId const nearby_point = _findNearestPoint(x_media, y_media, _selection_threshold);
    if (nearby_point != EntityId(0)) {
        _selectPoint(nearby_point);
    } else {
        _clearPointSelection();
    }
}

EntityId MediaPoint_Widget::_findNearestPoint(qreal x_media, qreal y_media, float max_distance) {
    if (_active_key.empty())
        return EntityId(0);

    auto point_data = _data_manager->getData<PointData>(_active_key);
    if (!point_data)
        return EntityId(0);

    auto time_position = _state->current_position;

    auto point_time_index = time_position.convertTo(point_data->getTimeFrame().get());

    auto points = point_data->getAtTime(point_time_index);
    if (points.empty())
        return EntityId(0);
    float min_distance = max_distance;
    int closest_point_index = -1;
    for (auto const & point: points) {
        float const dx = point.x - static_cast<float>(x_media);
        float const dy = point.y - static_cast<float>(y_media);
        float const distance = std::sqrt(dx * dx + dy * dy);

        if (distance < min_distance) {
            min_distance = distance;
            closest_point_index = &point - &points[0];// Get index of the closest point
        }
    }

    if (closest_point_index != -1) {
        auto entity_ids = point_data->getEntityIdsAtTime(point_time_index);
        if (closest_point_index < static_cast<int>(entity_ids.size())) {
            return entity_ids[closest_point_index];
        }
    }

    return EntityId(0);
}

void MediaPoint_Widget::_selectPoint(EntityId point_id) {
    _selected_point_id = point_id;

    // Use Media_Window's selection system for visual feedback
    _scene->selectEntity(point_id, _active_key, "point");
    std::cout << "Selected point with ID: " << point_id.id << std::endl;
}

void MediaPoint_Widget::_clearPointSelection() {
    if (_selected_point_id != EntityId(0)) {
        _selected_point_id = EntityId(0);
        _scene->clearAllSelections();// Clear all selections in scene
        _scene->UpdateCanvas();      // Refresh to remove selection highlight
        std::cout << "Cleared point selection" << std::endl;
    }
}

void MediaPoint_Widget::_moveSelectedPoint(qreal x_media, qreal y_media) {
    if (_selected_point_id == EntityId(0) || _active_key.empty())
        return;

    auto point_data = _data_manager->getData<PointData>(_active_key);
    if (!point_data) {
        return;
    }

    // Modify the selected point via EntityId using the PointData modification handle
    auto point_handle_opt = point_data->getMutableData(_selected_point_id, NotifyObservers::Yes);
    if (!point_handle_opt.has_value()) {
        std::cout << "Could not get mutable reference to point with EntityID " << _selected_point_id.id << std::endl;
        return;
    }

    Point2D<float> & point_ref = point_handle_opt.value().get();
    point_ref.x = static_cast<float>(x_media);
    point_ref.y = static_cast<float>(y_media);

    _scene->UpdateCanvas();
    std::cout << "Moved point (EntityID: " << _selected_point_id.id << ") to: (" << x_media << ", " << y_media << ")" << std::endl;
}

void MediaPoint_Widget::_assignPoint(qreal x_media, qreal y_media) {
    // Legacy method - now just calls the move function for compatibility
    _moveSelectedPoint(x_media, y_media);
}

void MediaPoint_Widget::_addPointAtCurrentTime(qreal x_media, qreal y_media) {
    if (_active_key.empty())
        return;

    auto point_data = _data_manager->getData<PointData>(_active_key);

    auto time_position = _state->current_position;
    auto point_time_index = time_position.convertTo(point_data->getTimeFrame().get());

    if (point_data) {
        Point2D<float> const new_point(static_cast<float>(x_media), static_cast<float>(y_media));
        point_data->addAtTime(point_time_index, new_point, NotifyObservers::No);
        std::cout << "Added new point at: (" << x_media << ", " << y_media << ") at time "
                  << point_time_index.getValue() << std::endl;
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
