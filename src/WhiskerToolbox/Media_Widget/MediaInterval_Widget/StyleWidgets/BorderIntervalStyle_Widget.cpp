#include "BorderIntervalStyle_Widget.hpp"
#include "ui_BorderIntervalStyle_Widget.h"

#include "DataManager/DataManager.hpp"
#include "Media_Widget/DisplayOptions/DisplayOptions.hpp"
#include "Media_Window/Media_Window.hpp"

#include <string>

BorderIntervalStyle_Widget::BorderIntervalStyle_Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::BorderIntervalStyle_Widget) {
    ui->setupUi(this);

    // Connect controls to slots
    connect(ui->border_thickness_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &BorderIntervalStyle_Widget::_setBorderThickness);
}

BorderIntervalStyle_Widget::~BorderIntervalStyle_Widget() {
    delete ui;
}

void BorderIntervalStyle_Widget::setActiveKey(std::string const & key) {
    _active_key = key;
}

void BorderIntervalStyle_Widget::setScene(Media_Window * scene) {
    _scene = scene;
}

void BorderIntervalStyle_Widget::updateFromConfig(DigitalIntervalDisplayOptions const * config) {
    if (!config) return;

    // Update controls with config values
    ui->border_thickness_spinbox->setValue(config->border_thickness);
}

void BorderIntervalStyle_Widget::_setBorderThickness(int thickness) {
    if (!_active_key.empty()) {
        auto interval_opts = _scene->getIntervalConfig(_active_key);
        if (interval_opts.has_value()) {
            interval_opts.value()->border_thickness = thickness;
        }
        _scene->UpdateCanvas();
        emit configChanged();
    }
} 