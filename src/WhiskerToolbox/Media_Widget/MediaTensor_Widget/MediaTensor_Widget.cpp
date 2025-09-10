#include "MediaTensor_Widget.hpp"
#include "ui_MediaTensor_Widget.h"

#include "DataManager/DataManager.hpp"
#include "Media_Widget/Media_Window/Media_Window.hpp"

//https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots
#include "DataManager/Tensors/Tensor_Data.hpp"
#define slots Q_SLOTS

#include <iostream>

MediaTensor_Widget::MediaTensor_Widget(std::shared_ptr<DataManager> data_manager, Media_Window * scene, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::MediaTensor_Widget),
      _data_manager{std::move(data_manager)},
      _scene{std::move(scene)} {
    ui->setupUi(this);

    connect(ui->horizontalSlider, &QSlider::valueChanged, this, &MediaTensor_Widget::_setTensorChannel);
    connect(ui->color_picker, &ColorPicker_Widget::colorChanged, this, &MediaTensor_Widget::_setTensorColor);
    connect(ui->color_picker, &ColorPicker_Widget::alphaChanged, this, &MediaTensor_Widget::_setTensorAlpha);
}

MediaTensor_Widget::~MediaTensor_Widget() {
    delete ui;
}

void MediaTensor_Widget::setActiveKey(std::string const & key) {
    _active_key = key;
    ui->name_label->setText(QString::fromStdString(key));

    if (_data_manager && !_active_key.empty()) {
        auto tensor_data = _data_manager->getData<TensorData>(_active_key);
        if (tensor_data) {
            auto shape = tensor_data->getFeatureShape();
            ui->horizontalSlider->setMaximum(static_cast<int>(shape.back()) - 1);

            ui->horizontalSlider->setValue(0);
            
            // Set the color picker to the current tensor color if available
            auto config = _scene->getTensorConfig(key);
            if (config) {
                ui->color_picker->setColor(QString::fromStdString(config.value()->hex_color));
                ui->color_picker->setAlpha(static_cast<int>(config.value()->alpha * 100));
            }
        }
    }
}

void MediaTensor_Widget::_setTensorChannel(int channel) {
    if (!_active_key.empty() && _scene) {
        auto opts = _scene->getTensorConfig(_active_key);
        if (opts.has_value()) {
            opts.value()->display_channel = channel;
        }
        _scene->UpdateCanvas();
    }
}

void MediaTensor_Widget::_setTensorColor(const QString& hex_color) {
    if (!_active_key.empty()) {
        auto tensor_opts = _scene->getTensorConfig(_active_key);
        if (tensor_opts.has_value()) {
            tensor_opts.value()->hex_color = hex_color.toStdString();
        }
        _scene->UpdateCanvas();
    }
}

void MediaTensor_Widget::_setTensorAlpha(int alpha) {
    float const alpha_float = static_cast<float>(alpha) / 100;

    if (!_active_key.empty()) {
        auto tensor_opts = _scene->getTensorConfig(_active_key);
        if (tensor_opts.has_value()) {
            tensor_opts.value()->alpha = alpha_float;
        }
        _scene->UpdateCanvas();
    }
}
