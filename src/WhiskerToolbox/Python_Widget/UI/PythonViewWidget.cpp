#include "PythonViewWidget.hpp"
#include "ui_PythonViewWidget.h"

#include "Core/PythonWidgetState.hpp"

PythonViewWidget::PythonViewWidget(std::shared_ptr<PythonWidgetState> state,
                                   QWidget * parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::PythonViewWidget>())
    , _state(std::move(state)) {
    ui->setupUi(this);
}

PythonViewWidget::~PythonViewWidget() = default;
