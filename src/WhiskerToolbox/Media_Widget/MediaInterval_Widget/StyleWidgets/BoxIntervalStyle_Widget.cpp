#include "BoxIntervalStyle_Widget.hpp"
#include "ui_BoxIntervalStyle_Widget.h"

#include "DataManager/DataManager.hpp"
#include "Media_Widget/DisplayOptions/DisplayOptions.hpp"
#include "Media_Window/Media_Window.hpp"

#include <string>

BoxIntervalStyle_Widget::BoxIntervalStyle_Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::BoxIntervalStyle_Widget) {
    ui->setupUi(this);

    // Connect controls to slots
    connect(ui->box_size_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &BoxIntervalStyle_Widget::_setBoxSize);
    connect(ui->frame_range_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &BoxIntervalStyle_Widget::_setFrameRange);
    connect(ui->location_combobox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BoxIntervalStyle_Widget::_setLocation);

    // Set default combobox selection to Top Right
    ui->location_combobox->setCurrentIndex(1);
}

BoxIntervalStyle_Widget::~BoxIntervalStyle_Widget() {
    delete ui;
}

void BoxIntervalStyle_Widget::setActiveKey(std::string const & key) {
    _active_key = key;
}

void BoxIntervalStyle_Widget::setScene(Media_Window * scene) {
    _scene = scene;
}

void BoxIntervalStyle_Widget::updateFromConfig(DigitalIntervalDisplayOptions const * config) {
    if (!config) return;

    // Update controls with config values
    ui->box_size_spinbox->setValue(config->box_size);
    ui->frame_range_spinbox->setValue(config->frame_range);
    ui->location_combobox->setCurrentIndex(static_cast<int>(config->location));
}

void BoxIntervalStyle_Widget::_setBoxSize(int size) {
    if (!_active_key.empty()) {
        auto interval_opts = _scene->getIntervalConfig(_active_key);
        if (interval_opts.has_value()) {
            interval_opts.value()->box_size = size;
        }
        _scene->UpdateCanvas();
        emit configChanged();
    }
}

void BoxIntervalStyle_Widget::_setFrameRange(int range) {
    if (!_active_key.empty()) {
        auto interval_opts = _scene->getIntervalConfig(_active_key);
        if (interval_opts.has_value()) {
            interval_opts.value()->frame_range = range;
        }
        _scene->UpdateCanvas();
        emit configChanged();
    }
}

void BoxIntervalStyle_Widget::_setLocation(int location_index) {
    if (!_active_key.empty()) {
        auto interval_opts = _scene->getIntervalConfig(_active_key);
        if (interval_opts.has_value()) {
            interval_opts.value()->location = static_cast<IntervalLocation>(location_index);
        }
        _scene->UpdateCanvas();
        emit configChanged();
    }
} 