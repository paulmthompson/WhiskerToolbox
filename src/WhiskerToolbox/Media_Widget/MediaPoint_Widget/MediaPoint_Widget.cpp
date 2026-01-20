#include "MediaPoint_Widget.hpp"
#include "ui_MediaPoint_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "Media_Widget/Media_Window/Media_Window.hpp"
#include "Media_Widget/DisplayOptions/DisplayOptions.hpp"

#include <iostream>
#include <cmath>
#include <QPointF>

MediaPoint_Widget::MediaPoint_Widget(std::shared_ptr<DataManager> data_manager, Media_Window * scene, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::MediaPoint_Widget),
      _data_manager{std::move(data_manager)},
      _scene{scene}
{
    ui->setupUi(this);

    connect(ui->color_picker, &ColorPicker_Widget::colorChanged,
            this, &MediaPoint_Widget::_setPointColor);
    connect(ui->color_picker, &ColorPicker_Widget::alphaChanged,
            this, &MediaPoint_Widget::_setPointAlpha);
    
    // Connect point size controls
    connect(ui->point_size_slider, &QSlider::valueChanged,
            this, &MediaPoint_Widget::_setPointSize);
    connect(ui->point_size_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MediaPoint_Widget::_setPointSize);
    
    // Connect marker shape control
    connect(ui->marker_shape_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MediaPoint_Widget::_setMarkerShape);
    
    // Synchronize slider and spinbox
    connect(ui->point_size_slider, &QSlider::valueChanged,
            ui->point_size_spinbox, &QSpinBox::setValue);
    connect(ui->point_size_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            ui->point_size_slider, &QSlider::setValue);
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
    disconnect(_scene, &Media_Window::leftClickMediaWithEvent, this, &MediaPoint_Widget::_handlePointClickWithModifiers);
    _clearPointSelection(); // Clear selection when widget is hidden
}

void MediaPoint_Widget::setActiveKey(std::string const & key) {
    _active_key = key;
    ui->name_label->setText(QString::fromStdString(key));
    _selection_enabled = !key.empty();

    // Set the color picker to the current point color if available
    if (!key.empty()) {
        auto config = _scene->getPointConfig(key);

        if (config) {
            ui->color_picker->setColor(QString::fromStdString(config.value()->hex_color()));
            ui->color_picker->setAlpha(static_cast<int>(config.value()->alpha() * 100));
            
            // Set point size controls
            ui->point_size_slider->blockSignals(true);
            ui->point_size_spinbox->blockSignals(true);
            ui->point_size_slider->setValue(config.value()->point_size);
            ui->point_size_spinbox->setValue(config.value()->point_size);
            ui->point_size_slider->blockSignals(false);
            ui->point_size_spinbox->blockSignals(false);
            
            // Set marker shape control
            ui->marker_shape_combo->blockSignals(true);
            ui->marker_shape_combo->setCurrentIndex(static_cast<int>(config.value()->marker_shape));
            ui->marker_shape_combo->blockSignals(false);
        }
    }
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
    EntityId nearby_point = _findNearestPoint(x_media, y_media, _selection_threshold);
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
    
    auto current_time = _data_manager->getCurrentTime();
    
    // Handle timeframe conversion if necessary
    auto video_timeframe = _data_manager->getTime(TimeKey("time"));
    auto point_timeframe_key = _data_manager->getTimeKey(_active_key);
    
    if (!point_timeframe_key.empty()) {
        auto point_timeframe = _data_manager->getTime(point_timeframe_key);
        if (video_timeframe.get() != point_timeframe.get()) {
            current_time = video_timeframe->getTimeAtIndex(TimeFrameIndex(current_time));
            current_time = point_timeframe->getIndexAtTime(current_time).getValue();
        }
    }
    
    auto points = point_data->getAtTime(TimeFrameIndex(current_time));
    if (points.empty())
        return EntityId(0);
    float min_distance = max_distance;
    int closest_point_index = -1;
    for (const auto& point : points) {
        float dx = point.x - static_cast<float>(x_media);
        float dy = point.y - static_cast<float>(y_media);
        float distance = std::sqrt(dx * dx + dy * dy);

        if (distance < min_distance) {
            min_distance = distance;
            closest_point_index = &point - &points[0]; // Get index of the closest point
        }
        
    }

    if (closest_point_index != -1) {
        auto entity_ids = point_data->getEntityIdsAtTime(TimeFrameIndex(current_time));
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
        _scene->clearAllSelections(); // Clear all selections in scene
        _scene->UpdateCanvas(); // Refresh to remove selection highlight
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
    
    auto current_time = _data_manager->getCurrentTime();
    
    // Handle timeframe conversion if necessary
    auto video_timeframe = _data_manager->getTime(TimeKey("time"));
    auto point_timeframe_key = _data_manager->getTimeKey(_active_key);
    
    if (!point_timeframe_key.empty()) {
        auto point_timeframe = _data_manager->getTime(point_timeframe_key);
        if (video_timeframe.get() != point_timeframe.get()) {
            current_time = video_timeframe->getTimeAtIndex(TimeFrameIndex(current_time));
            current_time = point_timeframe->getIndexAtTime(current_time).getValue();
        }
    }
    
    auto point_data = _data_manager->getData<PointData>(_active_key);
    if (point_data) {
        Point2D<float> new_point(static_cast<float>(x_media), static_cast<float>(y_media));
        point_data->addAtTime(TimeFrameIndex(current_time), new_point, NotifyObservers::No);
        std::cout << "Added new point at: (" << x_media << ", " << y_media << ") at time " << current_time << std::endl;
        point_data->notifyObservers();
    }
}

void MediaPoint_Widget::_setPointColor(const QString& hex_color) {
    if (!_active_key.empty()) {
        auto point_opts = _scene->getPointConfig(_active_key);
        if (point_opts.has_value()) {
            point_opts.value()->hex_color() = hex_color.toStdString();
        }
        _scene->UpdateCanvas();
    }
}

void MediaPoint_Widget::_setPointAlpha(int alpha) {
    float const alpha_float = static_cast<float>(alpha) / 100;

    if (!_active_key.empty()) {
        auto point_opts = _scene->getPointConfig(_active_key);
        if (point_opts.has_value()) {
            point_opts.value()->alpha() = alpha_float;
        }
        _scene->UpdateCanvas();
    }
}

void MediaPoint_Widget::_setPointSize(int size) {
    if (!_active_key.empty()) {
        auto point_opts = _scene->getPointConfig(_active_key);
        if (point_opts.has_value()) {
            point_opts.value()->point_size = size;
        }
        _scene->UpdateCanvas();
    }
    
    // Synchronize slider and spinbox if the signal came from one of them
    QObject* sender_obj = sender();
    if (sender_obj == ui->point_size_slider) {
        ui->point_size_spinbox->blockSignals(true);
        ui->point_size_spinbox->setValue(size);
        ui->point_size_spinbox->blockSignals(false);
    } else if (sender_obj == ui->point_size_spinbox) {
        ui->point_size_slider->blockSignals(true);
        ui->point_size_slider->setValue(size);
        ui->point_size_slider->blockSignals(false);
    }
}

void MediaPoint_Widget::_setMarkerShape(int shapeIndex) {
    if (!_active_key.empty() && shapeIndex >= 0) {
        auto point_opts = _scene->getPointConfig(_active_key);
        if (point_opts.has_value()) {
            point_opts.value()->marker_shape = static_cast<PointMarkerShape>(shapeIndex);
        }
        _scene->UpdateCanvas();
    }
}
