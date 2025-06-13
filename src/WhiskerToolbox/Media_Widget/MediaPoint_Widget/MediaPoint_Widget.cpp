#include "MediaPoint_Widget.hpp"
#include "ui_MediaPoint_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "Media_Window/Media_Window.hpp"
#include "Media_Widget/DisplayOptions/DisplayOptions.hpp"

#include <iostream>

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
    connect(_scene, &Media_Window::leftClickMedia, this, &MediaPoint_Widget::_assignPoint);

}

void MediaPoint_Widget::hideEvent(QHideEvent * event) {

    static_cast<void>(event);

    std::cout << "Hide Event" << std::endl;
    disconnect(_scene, &Media_Window::leftClickMedia, this, &MediaPoint_Widget::_assignPoint);
}

void MediaPoint_Widget::setActiveKey(std::string const & key) {
    _active_key = key;
    ui->name_label->setText(QString::fromStdString(key));
    _selection_enabled = !key.empty();

    // Set the color picker to the current point color if available
    if (!key.empty()) {
        auto config = _scene->getPointConfig(key);

        if (config) {
            ui->color_picker->setColor(QString::fromStdString(config.value()->hex_color));
            ui->color_picker->setAlpha(static_cast<int>(config.value()->alpha * 100));
            
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


void MediaPoint_Widget::_assignPoint(qreal x_media, qreal y_media) {
    if (!_selection_enabled || _active_key.empty())
        return;

    auto current_time = _data_manager->getCurrentTime();

    auto point = _data_manager->getData<PointData>(_active_key);
    if (point) {

        point->overwritePointAtTime(current_time, {.x = static_cast<float>(x_media),
                                               .y = static_cast<float>(y_media)
                                              });

        _scene->UpdateCanvas();
    }
}

void MediaPoint_Widget::_setPointColor(const QString& hex_color) {
    if (!_active_key.empty()) {
        auto point_opts = _scene->getPointConfig(_active_key);
        if (point_opts.has_value()) {
            point_opts.value()->hex_color = hex_color.toStdString();
        }
        _scene->UpdateCanvas();
    }
}

void MediaPoint_Widget::_setPointAlpha(int alpha) {
    float const alpha_float = static_cast<float>(alpha) / 100;

    if (!_active_key.empty()) {
        auto point_opts = _scene->getPointConfig(_active_key);
        if (point_opts.has_value()) {
            point_opts.value()->alpha = alpha_float;
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
