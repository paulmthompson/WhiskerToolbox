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

    connect(ui->alpha_slider, &QSlider::valueChanged, this, &MediaMask_Widget::_setMaskAlpha);

}

MediaMask_Widget::~MediaMask_Widget() {
    delete ui;
}

void MediaMask_Widget::setActiveKey(std::string const & key) {
    _active_key = key;
    ui->name_label->setText(QString::fromStdString(key));

    if (_scene && !_active_key.empty()) {
        ui->alpha_slider->setValue(100);
    }
}


void MediaMask_Widget::_setMaskAlpha(int alpha) {
    float const alpha_float = static_cast<float>(alpha) / 100;

    if (!_active_key.empty()) {
        auto mask_opts = _scene->getLineConfig(_active_key);
        if (mask_opts.has_value()) {
            mask_opts.value()->alpha = alpha_float;
        }
        _scene->UpdateCanvas();
    }
}
