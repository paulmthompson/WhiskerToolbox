#include "MediaTensor_Widget.hpp"
#include "ui_MediaTensor_Widget.h"

#include "DataManager/DataManager.hpp"
#include "Media_Window/Media_Window.hpp"

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
