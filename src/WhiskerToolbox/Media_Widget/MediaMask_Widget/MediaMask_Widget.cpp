#include "MediaMask_Widget.hpp"
#include "ui_MediaMask_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "Media_Window/Media_Window.hpp"

#include <iostream>

MediaMask_Widget::MediaMask_Widget(std::shared_ptr<DataManager> data_manager, Media_Window * scene, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::MediaMask_Widget),
      _data_manager{std::move(data_manager)},
      _scene{std::move(scene)} {
    ui->setupUi(this);

    connect(ui->color_picker, &ColorPicker_Widget::alphaChanged,
            this, &MediaMask_Widget::_setMaskAlpha);
    connect(ui->color_picker, &ColorPicker_Widget::colorChanged,
            this, &MediaMask_Widget::_setMaskColor);
}

MediaMask_Widget::~MediaMask_Widget() {
    delete ui;
}

void MediaMask_Widget::setActiveKey(std::string const & key) {
    _active_key = key;
    ui->name_label->setText(QString::fromStdString(key));

    // Set the color picker to the current mask color if available
    if (!key.empty()) {
        auto config = _scene->getMaskConfig(key);

        if (config) {
            ui->color_picker->setColor(QString::fromStdString(config.value()->hex_color));
            ui->color_picker->setAlpha(static_cast<int>(config.value()->alpha * 100));
        }
    }
}

void MediaMask_Widget::_setMaskAlpha(int alpha) {
    float const alpha_float = static_cast<float>(alpha) / 100;

    if (!_active_key.empty()) {
        auto mask_opts = _scene->getMaskConfig(_active_key);
        if (mask_opts.has_value()) {
            mask_opts.value()->alpha = alpha_float;
        }
        _scene->UpdateCanvas();
    }
}

void MediaMask_Widget::_setMaskColor(const QString& hex_color) {
    if (!_active_key.empty()) {
        auto mask_opts = _scene->getMaskConfig(_active_key);
        if (mask_opts.has_value()) {
            mask_opts.value()->hex_color = hex_color.toStdString();
        }
        _scene->UpdateCanvas();
    }
}
