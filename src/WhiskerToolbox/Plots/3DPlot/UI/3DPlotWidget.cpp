#include "3DPlotWidget.hpp"

#include "Core/3DPlotState.hpp"
#include "DataManager/DataManager.hpp"

#include <QResizeEvent>
#include <QVBoxLayout>

#include "ui_3DPlotWidget.h"

ThreeDPlotWidget::ThreeDPlotWidget(std::shared_ptr<DataManager> data_manager,
                                   QWidget * parent)
    : QWidget(parent),
      _data_manager(data_manager),
      ui(new Ui::ThreeDPlotWidget),
      _state(nullptr) {
    ui->setupUi(this);

    // Widget is empty for now - will be populated later
}

ThreeDPlotWidget::~ThreeDPlotWidget() {
    delete ui;
}

void ThreeDPlotWidget::setState(std::shared_ptr<ThreeDPlotState> state) {
    _state = state;

    // Connect state signals if needed
    // Widget is empty for now
}

void ThreeDPlotWidget::resizeEvent(QResizeEvent * event) {
    QWidget::resizeEvent(event);
    // Handle resize if needed
}

ThreeDPlotState * ThreeDPlotWidget::state() {
    return _state.get();
}
