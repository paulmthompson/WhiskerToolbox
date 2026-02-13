#include "PythonPropertiesWidget.hpp"
#include "ui_PythonPropertiesWidget.h"

#include "Core/PythonWidgetState.hpp"

PythonPropertiesWidget::PythonPropertiesWidget(std::shared_ptr<PythonWidgetState> state,
                                               QWidget * parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::PythonPropertiesWidget>())
    , _state(std::move(state)) {
    ui->setupUi(this);
}

PythonPropertiesWidget::~PythonPropertiesWidget() = default;
