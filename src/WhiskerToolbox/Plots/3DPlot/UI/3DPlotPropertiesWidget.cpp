#include "3DPlotPropertiesWidget.hpp"

#include "Core/3DPlotState.hpp"
#include "DataManager/DataManager.hpp"
#include "UI/3DPlotWidget.hpp"

#include "ui_3DPlotPropertiesWidget.h"

ThreeDPlotPropertiesWidget::ThreeDPlotPropertiesWidget(std::shared_ptr<ThreeDPlotState> state,
                                                       std::shared_ptr<DataManager> data_manager,
                                                       QWidget * parent)
    : QWidget(parent),
      ui(new Ui::ThreeDPlotPropertiesWidget),
      _state(state),
      _data_manager(data_manager),
      _plot_widget(nullptr) {
    ui->setupUi(this);

    // Connect state signals if needed
    if (_state) {
        // Initialize UI from state
        _updateUIFromState();
    }
}

void ThreeDPlotPropertiesWidget::setPlotWidget(ThreeDPlotWidget * plot_widget) {
    _plot_widget = plot_widget;

    if (!_plot_widget) {
        return;
    }

    // Connect plot widget controls if needed
    // Widget is empty for now
}

ThreeDPlotPropertiesWidget::~ThreeDPlotPropertiesWidget() {
    delete ui;
}

void ThreeDPlotPropertiesWidget::_updateUIFromState() {
    if (!_state) {
        return;
    }

    // Update UI elements from state
    // Widget is empty for now
}
