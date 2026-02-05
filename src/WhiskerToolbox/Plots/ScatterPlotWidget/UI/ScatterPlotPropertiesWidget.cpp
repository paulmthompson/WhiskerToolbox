#include "ScatterPlotPropertiesWidget.hpp"

#include "Core/ScatterPlotState.hpp"
#include "DataManager/DataManager.hpp"

#include "ui_ScatterPlotPropertiesWidget.h"

ScatterPlotPropertiesWidget::ScatterPlotPropertiesWidget(std::shared_ptr<ScatterPlotState> state,
                                                          std::shared_ptr<DataManager> data_manager,
                                                          QWidget * parent)
    : QWidget(parent),
      ui(new Ui::ScatterPlotPropertiesWidget),
      _state(state),
      _data_manager(data_manager)
{
    ui->setupUi(this);

    // Connect spinboxes to slots
    connect(ui->x_min_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ScatterPlotPropertiesWidget::_onXMinChanged);
    connect(ui->x_max_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ScatterPlotPropertiesWidget::_onXMaxChanged);
    connect(ui->y_min_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ScatterPlotPropertiesWidget::_onYMinChanged);
    connect(ui->y_max_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ScatterPlotPropertiesWidget::_onYMaxChanged);

    // Connect state signals to update UI
    if (_state) {
        connect(_state.get(), &ScatterPlotState::xMinChanged,
                this, [this](double x_min) {
                    ui->x_min_spinbox->blockSignals(true);
                    ui->x_min_spinbox->setValue(x_min);
                    ui->x_min_spinbox->blockSignals(false);
                });
        connect(_state.get(), &ScatterPlotState::xMaxChanged,
                this, [this](double x_max) {
                    ui->x_max_spinbox->blockSignals(true);
                    ui->x_max_spinbox->setValue(x_max);
                    ui->x_max_spinbox->blockSignals(false);
                });
        connect(_state.get(), &ScatterPlotState::yMinChanged,
                this, [this](double y_min) {
                    ui->y_min_spinbox->blockSignals(true);
                    ui->y_min_spinbox->setValue(y_min);
                    ui->y_min_spinbox->blockSignals(false);
                });
        connect(_state.get(), &ScatterPlotState::yMaxChanged,
                this, [this](double y_max) {
                    ui->y_max_spinbox->blockSignals(true);
                    ui->y_max_spinbox->setValue(y_max);
                    ui->y_max_spinbox->blockSignals(false);
                });

        // Initialize UI from state
        _updateUIFromState();
    }
}

ScatterPlotPropertiesWidget::~ScatterPlotPropertiesWidget()
{
    delete ui;
}

void ScatterPlotPropertiesWidget::_updateUIFromState()
{
    if (!_state) {
        return;
    }

    ui->x_min_spinbox->blockSignals(true);
    ui->x_min_spinbox->setValue(_state->getXMin());
    ui->x_min_spinbox->blockSignals(false);

    ui->x_max_spinbox->blockSignals(true);
    ui->x_max_spinbox->setValue(_state->getXMax());
    ui->x_max_spinbox->blockSignals(false);

    ui->y_min_spinbox->blockSignals(true);
    ui->y_min_spinbox->setValue(_state->getYMin());
    ui->y_min_spinbox->blockSignals(false);

    ui->y_max_spinbox->blockSignals(true);
    ui->y_max_spinbox->setValue(_state->getYMax());
    ui->y_max_spinbox->blockSignals(false);
}

void ScatterPlotPropertiesWidget::_onXMinChanged(double value)
{
    if (_state) {
        _state->setXMin(value);
    }
}

void ScatterPlotPropertiesWidget::_onXMaxChanged(double value)
{
    if (_state) {
        _state->setXMax(value);
    }
}

void ScatterPlotPropertiesWidget::_onYMinChanged(double value)
{
    if (_state) {
        _state->setYMin(value);
    }
}

void ScatterPlotPropertiesWidget::_onYMaxChanged(double value)
{
    if (_state) {
        _state->setYMax(value);
    }
}
