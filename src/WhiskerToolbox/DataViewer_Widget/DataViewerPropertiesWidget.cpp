#include "DataViewerPropertiesWidget.hpp"
#include "ui_DataViewerPropertiesWidget.h"

#include "DataManager/DataManager.hpp"
#include "DataViewerState.hpp"
#include "DataViewerStateData.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QSpinBox>

#include <iostream>

DataViewerPropertiesWidget::DataViewerPropertiesWidget(std::shared_ptr<DataViewerState> state,
                                                         std::shared_ptr<DataManager> data_manager,
                                                         QWidget * parent)
    : QWidget(parent),
      ui(new Ui::DataViewerPropertiesWidget),
      _state(std::move(state)),
      _data_manager(std::move(data_manager)) {
    ui->setupUi(this);

    _initializeFromState();
    _connectUIControls();
    _connectStateSignals();
}

DataViewerPropertiesWidget::~DataViewerPropertiesWidget() {
    delete ui;
}

void DataViewerPropertiesWidget::setXAxisSamplesMaximum(int max) {
    ui->x_axis_samples->setMaximum(max);
}

void DataViewerPropertiesWidget::_initializeFromState() {
    if (!_state) {
        return;
    }
    
    _updating_from_state = true;
    
    // Theme
    int theme_index = 0;
    switch (_state->theme()) {
        case DataViewerTheme::Dark:
            theme_index = 0;
            break;
        case DataViewerTheme::Light:
            theme_index = 2;
            break;
        default:
            theme_index = 0;
            break;
    }
    ui->theme_combo->setCurrentIndex(theme_index);
    
    // Global zoom
    ui->global_zoom->setValue(static_cast<double>(_state->globalZoom()));
    
    // X axis samples (time width from view state)
    auto const & view = _state->viewState();
    ui->x_axis_samples->setValue(static_cast<int>(view.getTimeWidth()));
    
    // Grid settings
    ui->grid_lines_enabled->setChecked(_state->gridEnabled());
    ui->grid_spacing->setValue(_state->gridSpacing());
    
    _updating_from_state = false;
}

void DataViewerPropertiesWidget::_connectUIControls() {
    // Theme combo box
    connect(ui->theme_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DataViewerPropertiesWidget::_onThemeChanged);
    
    // Global zoom
    connect(ui->global_zoom, &QDoubleSpinBox::valueChanged,
            this, &DataViewerPropertiesWidget::_onGlobalZoomChanged);
    
    // X axis samples
    connect(ui->x_axis_samples, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &DataViewerPropertiesWidget::_onXAxisSamplesChanged);
    
    // Grid controls
    connect(ui->grid_lines_enabled, &QCheckBox::toggled,
            this, &DataViewerPropertiesWidget::_onGridLinesToggled);
    connect(ui->grid_spacing, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &DataViewerPropertiesWidget::_onGridSpacingChanged);
    
    // Auto-arrange button
    connect(ui->auto_arrange_button, &QPushButton::clicked,
            this, &DataViewerPropertiesWidget::autoArrangeRequested);
    
    // Export SVG button
    connect(ui->export_svg_button, &QPushButton::clicked, this, [this]() {
        bool const includeScalebar = ui->svg_scalebar_checkbox->isChecked();
        int const scalebarLength = ui->scalebar_length_spinbox->value();
        emit exportSVGRequested(includeScalebar, scalebarLength);
    });
    
    // Scalebar checkbox enables/disables the length spinbox
    connect(ui->svg_scalebar_checkbox, &QCheckBox::toggled, this, [this](bool checked) {
        ui->scalebar_length_spinbox->setEnabled(checked);
        ui->scalebar_length_label->setEnabled(checked);
    });
}

void DataViewerPropertiesWidget::_connectStateSignals() {
    if (!_state) {
        return;
    }

    // Update theme combo when state changes
    connect(_state.get(), &DataViewerState::themeChanged, this, [this]() {
        if (_updating_from_state) return;
        _updating_from_state = true;
        
        int theme_index = 0;
        switch (_state->theme()) {
            case DataViewerTheme::Dark:
                theme_index = 0;
                break;
            case DataViewerTheme::Light:
                theme_index = 2;
                break;
            default:
                theme_index = 0;
                break;
        }
        ui->theme_combo->setCurrentIndex(theme_index);
        
        _updating_from_state = false;
    });

    // Update grid controls when state changes
    connect(_state.get(), &DataViewerState::gridChanged, this, [this]() {
        if (_updating_from_state) return;
        _updating_from_state = true;
        
        ui->grid_lines_enabled->setChecked(_state->gridEnabled());
        ui->grid_spacing->setValue(_state->gridSpacing());
        
        _updating_from_state = false;
    });

    // Update view controls when state changes
    connect(_state.get(), &DataViewerState::viewStateChanged, this, [this]() {
        if (_updating_from_state) return;
        _updating_from_state = true;
        
        ui->global_zoom->setValue(static_cast<double>(_state->globalZoom()));
        auto const & view = _state->viewState();
        ui->x_axis_samples->setValue(static_cast<int>(view.getTimeWidth()));
        
        _updating_from_state = false;
    });
}

void DataViewerPropertiesWidget::_onThemeChanged(int index) {
    if (_updating_from_state || !_state) return;
    
    DataViewerTheme theme = DataViewerTheme::Dark;
    switch (index) {
        case 0:
            theme = DataViewerTheme::Dark;
            break;
        case 1:
            // Purple theme - map to Dark for now (keeping UI consistent)
            theme = DataViewerTheme::Dark;
            break;
        case 2:
            theme = DataViewerTheme::Light;
            break;
        default:
            theme = DataViewerTheme::Dark;
            break;
    }
    
    _state->setTheme(theme);
}

void DataViewerPropertiesWidget::_onGlobalZoomChanged(double value) {
    if (_updating_from_state || !_state) return;
    
    _state->setGlobalZoom(static_cast<float>(value));
}

void DataViewerPropertiesWidget::_onXAxisSamplesChanged(int value) {
    if (_updating_from_state || !_state) return;
    
    _state->setTimeWidth(value);
}

void DataViewerPropertiesWidget::_onGridLinesToggled(bool enabled) {
    if (_updating_from_state || !_state) return;
    
    _state->setGridEnabled(enabled);
}

void DataViewerPropertiesWidget::_onGridSpacingChanged(int value) {
    if (_updating_from_state || !_state) return;
    
    _state->setGridSpacing(value);
}
