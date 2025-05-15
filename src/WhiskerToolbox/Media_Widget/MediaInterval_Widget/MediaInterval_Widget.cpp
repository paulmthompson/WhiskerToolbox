#include "MediaInterval_Widget.hpp"
#include "ui_MediaInterval_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Media_Window/Media_Window.hpp"

#include <iostream>

MediaInterval_Widget::MediaInterval_Widget(std::shared_ptr<DataManager> data_manager, Media_Window * scene, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::MediaInterval_Widget),
      _data_manager{std::move(data_manager)},
      _scene{std::move(scene)} {
    ui->setupUi(this);

    connect(ui->color_picker, &ColorPicker_Widget::alphaChanged,
            this, &MediaInterval_Widget::_setIntervalAlpha);
    connect(ui->color_picker, &ColorPicker_Widget::colorChanged,
            this, &MediaInterval_Widget::_setIntervalColor);
}

MediaInterval_Widget::~MediaInterval_Widget() {
    delete ui;
}

void MediaInterval_Widget::setActiveKey(std::string const & key) {
    _active_key = key;
    ui->name_label->setText(QString::fromStdString(key));

    // Set the color picker to the current interval color if available
    if (!key.empty()) {
        auto config = _scene->getIntervalConfig(key);

        if (config) {
            ui->color_picker->setColor(QString::fromStdString(config.value()->hex_color));
            ui->color_picker->setAlpha(static_cast<int>(config.value()->alpha * 100));
        }
    }
}

void MediaInterval_Widget::_setIntervalAlpha(int alpha) {
    float const alpha_float = static_cast<float>(alpha) / 100;

    if (!_active_key.empty()) {
        auto interval_opts = _scene->getIntervalConfig(_active_key);
        if (interval_opts.has_value()) {
            interval_opts.value()->alpha = alpha_float;
        }
        _scene->UpdateCanvas();
    }
}

void MediaInterval_Widget::_setIntervalColor(const QString& hex_color) {
    if (!_active_key.empty()) {
        auto interval_opts = _scene->getIntervalConfig(_active_key);
        if (interval_opts.has_value()) {
            interval_opts.value()->hex_color = hex_color.toStdString();
        }
        _scene->UpdateCanvas();
    }
}
