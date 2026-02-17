#include "TransformsV2Properties_Widget.hpp"
#include "ui_TransformsV2Properties_Widget.h"

#include "Core/TransformsV2State.hpp"

TransformsV2Properties_Widget::TransformsV2Properties_Widget(
        std::shared_ptr<TransformsV2State> state,
        QWidget * parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::TransformsV2Properties_Widget>())
    , _state(std::move(state)) {
    ui->setupUi(this);
}

TransformsV2Properties_Widget::~TransformsV2Properties_Widget() = default;
