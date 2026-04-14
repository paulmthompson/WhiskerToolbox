#include "MediaTensor_Widget.hpp"
#include "ui_MediaTensor_Widget.h"

#include "Media_Widget/Core/MediaWidgetState.hpp"
#include "Media_Widget/Rendering/Media_Window/Media_Window.hpp"

#include "ColorAlphaControls.hpp"
#include "DataManager/DataManager.hpp"
//https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots
#include "Tensors/TensorData.hpp"
#define slots Q_SLOTS

#include <iostream>

MediaTensor_Widget::MediaTensor_Widget(std::shared_ptr<DataManager> data_manager, Media_Window * scene, MediaWidgetState * state, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::MediaTensor_Widget),
      _data_manager{std::move(data_manager)},
      _scene{scene},
      _state{state} {
    ui->setupUi(this);

    connect(ui->horizontalSlider, &QSlider::valueChanged, this, &MediaTensor_Widget::_setTensorChannel);
    connect(ui->color_picker, &ColorAlphaControls::colorChanged, this, &MediaTensor_Widget::_setTensorColor);
    connect(ui->color_picker, &ColorAlphaControls::alphaChanged, this, &MediaTensor_Widget::_setTensorAlpha);
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
            // The channel slider selects among the last-axis elements (columns)
            auto const num_cols = tensor_data->numColumns();
            ui->horizontalSlider->setMaximum(static_cast<int>(num_cols) - 1);

            ui->horizontalSlider->setValue(0);

            // Set the color picker to the current tensor color if available
            if (_state) {
                auto const * config = _state->displayOptions().get<TensorDisplayOptions>(QString::fromStdString(key));
                if (config) {
                    ui->color_picker->setColor(QString::fromStdString(config->hex_color()));
                    ui->color_picker->setAlpha(config->alpha());
                }
            }
        }
    }
}

void MediaTensor_Widget::_setTensorChannel(int channel) {
    if (!_active_key.empty() && _scene && _state) {
        auto const key = QString::fromStdString(_active_key);
        auto * opts = _state->displayOptions().getMutable<TensorDisplayOptions>(key);
        if (opts) {
            opts->display_channel = channel;
            _state->displayOptions().notifyChanged<TensorDisplayOptions>(key);
        }
        _scene->UpdateCanvas();
    }
}

void MediaTensor_Widget::_setTensorColor(QString const & hex_color) {
    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        auto * tensor_opts = _state->displayOptions().getMutable<TensorDisplayOptions>(key);
        if (tensor_opts) {
            tensor_opts->hex_color() = hex_color.toStdString();
            _state->displayOptions().notifyChanged<TensorDisplayOptions>(key);
        }
        _scene->UpdateCanvas();
    }
}

void MediaTensor_Widget::_setTensorAlpha(float alpha) {

    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        auto * tensor_opts = _state->displayOptions().getMutable<TensorDisplayOptions>(key);
        if (tensor_opts) {
            tensor_opts->alpha() = alpha;
            _state->displayOptions().notifyChanged<TensorDisplayOptions>(key);
        }
        _scene->UpdateCanvas();
    }
}
