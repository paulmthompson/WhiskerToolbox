#include "DataViewerPropertiesWidget.hpp"
#include "ui_DataViewerPropertiesWidget.h"

#include "DataManager/DataManager.hpp"
#include "DataViewerState.hpp"

#include <iostream>

DataViewerPropertiesWidget::DataViewerPropertiesWidget(std::shared_ptr<DataViewerState> state,
                                                         std::shared_ptr<DataManager> data_manager,
                                                         QWidget * parent)
    : QWidget(parent),
      ui(new Ui::DataViewerPropertiesWidget),
      _state(std::move(state)),
      _data_manager(std::move(data_manager)) {
    ui->setupUi(this);

    _connectStateSignals();

    // TODO: Incrementally add controls from DataViewer_Widget left panel:
    // - Feature tree widget for selecting time series
    // - Theme selection (Dark/Light)
    // - Grid settings (enabled, spacing)
    // - X-axis samples control
    // - Global scale control
    // - Export to SVG button
}

DataViewerPropertiesWidget::~DataViewerPropertiesWidget() {
    delete ui;
}

void DataViewerPropertiesWidget::_connectStateSignals() {
    if (!_state) {
        return;
    }

    // Connect to state signals to update UI when state changes
    // These connections will be used as controls are added to the properties panel

    connect(_state.get(), &DataViewerState::themeChanged, this, []() {
        // TODO: Update theme controls when theme changes externally
    });

    connect(_state.get(), &DataViewerState::gridChanged, this, []() {
        // TODO: Update grid controls when grid settings change externally
    });

    connect(_state.get(), &DataViewerState::viewStateChanged, this, []() {
        // TODO: Update view controls when view state changes externally
    });

    connect(_state.get(), &DataViewerState::uiPreferencesChanged, this, []() {
        // TODO: Update UI preference controls when preferences change externally
    });
}
