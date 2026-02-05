#include "TemporalProjectionViewPropertiesWidget.hpp"

#include "Core/TemporalProjectionViewState.hpp"
#include "DataManager/DataManager.hpp"

#include "ui_TemporalProjectionViewPropertiesWidget.h"

TemporalProjectionViewPropertiesWidget::TemporalProjectionViewPropertiesWidget(
    std::shared_ptr<TemporalProjectionViewState> state,
    std::shared_ptr<DataManager> data_manager,
    QWidget * parent)
    : QWidget(parent),
      ui(new Ui::TemporalProjectionViewPropertiesWidget),
      _state(state),
      _data_manager(data_manager)
{
    ui->setupUi(this);

    // Connect spinboxes to slots
    connect(ui->x_min_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &TemporalProjectionViewPropertiesWidget::_onXMinChanged);
    connect(ui->x_max_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &TemporalProjectionViewPropertiesWidget::_onXMaxChanged);
    connect(ui->y_min_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &TemporalProjectionViewPropertiesWidget::_onYMinChanged);
    connect(ui->y_max_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &TemporalProjectionViewPropertiesWidget::_onYMaxChanged);

    // Connect state signals to update UI
    if (_state) {
        connect(_state.get(), &TemporalProjectionViewState::xMinChanged,
                this, [this](double x_min) {
                    ui->x_min_spinbox->blockSignals(true);
                    ui->x_min_spinbox->setValue(x_min);
                    ui->x_min_spinbox->blockSignals(false);
                });
        connect(_state.get(), &TemporalProjectionViewState::xMaxChanged,
                this, [this](double x_max) {
                    ui->x_max_spinbox->blockSignals(true);
                    ui->x_max_spinbox->setValue(x_max);
                    ui->x_max_spinbox->blockSignals(false);
                });
        connect(_state.get(), &TemporalProjectionViewState::yMinChanged,
                this, [this](double y_min) {
                    ui->y_min_spinbox->blockSignals(true);
                    ui->y_min_spinbox->setValue(y_min);
                    ui->y_min_spinbox->blockSignals(false);
                });
        connect(_state.get(), &TemporalProjectionViewState::yMaxChanged,
                this, [this](double y_max) {
                    ui->y_max_spinbox->blockSignals(true);
                    ui->y_max_spinbox->setValue(y_max);
                    ui->y_max_spinbox->blockSignals(false);
                });

        // Initialize UI from state
        _updateUIFromState();
    }
}

TemporalProjectionViewPropertiesWidget::~TemporalProjectionViewPropertiesWidget()
{
    delete ui;
}

void TemporalProjectionViewPropertiesWidget::_updateUIFromState()
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

void TemporalProjectionViewPropertiesWidget::_onXMinChanged(double value)
{
    if (_state) {
        _state->setXMin(value);
    }
}

void TemporalProjectionViewPropertiesWidget::_onXMaxChanged(double value)
{
    if (_state) {
        _state->setXMax(value);
    }
}

void TemporalProjectionViewPropertiesWidget::_onYMinChanged(double value)
{
    if (_state) {
        _state->setYMin(value);
    }
}

void TemporalProjectionViewPropertiesWidget::_onYMaxChanged(double value)
{
    if (_state) {
        _state->setYMax(value);
    }
}
