#include "ContrastWidget.hpp"
#include "ui_ContrastWidget.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QSpinBox>

ContrastWidget::ContrastWidget(QWidget* parent)
    : QWidget(parent),
      ui(new Ui::ContrastWidget) {
    
    ui->setupUi(this);

    // Connect UI controls to slots
    connect(ui->active_checkbox, &QCheckBox::toggled,
            this, &ContrastWidget::_onActiveChanged);
    connect(ui->alpha_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ContrastWidget::_onAlphaChanged);
    connect(ui->beta_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ContrastWidget::_onBetaChanged);
    connect(ui->display_min_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ContrastWidget::_onDisplayMinChanged);
    connect(ui->display_max_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ContrastWidget::_onDisplayMaxChanged);

    // Initialize ranges and values
    _updateSpinboxRanges();
}

ContrastWidget::~ContrastWidget() {
    delete ui;
}

ContrastOptions ContrastWidget::getOptions() const {
    ContrastOptions options;
    options.active = ui->active_checkbox->isChecked();
    options.alpha = ui->alpha_spinbox->value();
    options.beta = ui->beta_spinbox->value();
    options.display_min = ui->display_min_spinbox->value();
    options.display_max = ui->display_max_spinbox->value();
    return options;
}

void ContrastWidget::setOptions(ContrastOptions const& options) {
    _blockSignalsAndSetValues(options);
    _updateSpinboxRanges();
}

void ContrastWidget::_onActiveChanged() {
    _updateOptions();
}

void ContrastWidget::_onAlphaChanged() {
    if (_updating_values) return;

    // Calculate min/max from current alpha/beta and update those spinboxes
    ContrastOptions options = getOptions();
    options.calculateMinMaxFromAlphaBeta();
    _updateMinMaxFromAlphaBeta();
    _updateSpinboxRanges();
    _updateOptions();
}

void ContrastWidget::_onBetaChanged() {
    if (_updating_values) return;

    // Calculate min/max from current alpha/beta and update those spinboxes
    ContrastOptions options = getOptions();
    options.calculateMinMaxFromAlphaBeta();
    _updateMinMaxFromAlphaBeta();
    _updateSpinboxRanges();
    _updateOptions();
}

void ContrastWidget::_onDisplayMinChanged() {
    if (_updating_values) return;

    // Update max spinbox range and calculate alpha/beta from min/max
    _updateSpinboxRanges();
    ContrastOptions options = getOptions();
    options.calculateAlphaBetaFromMinMax();
    _updateAlphaBetaFromMinMax();
    _updateOptions();
}

void ContrastWidget::_onDisplayMaxChanged() {
    if (_updating_values) return;

    // Update min spinbox range and calculate alpha/beta from min/max
    _updateSpinboxRanges();
    ContrastOptions options = getOptions();
    options.calculateAlphaBetaFromMinMax();
    _updateAlphaBetaFromMinMax();
    _updateOptions();
}

void ContrastWidget::_updateAlphaBetaFromMinMax() {
    _updating_values = true;
    ContrastOptions options = getOptions();
    options.calculateAlphaBetaFromMinMax();

    // Update the alpha/beta display values without triggering signals
    ui->alpha_spinbox->setValue(options.alpha);
    ui->beta_spinbox->setValue(options.beta);

    _updating_values = false;
}

void ContrastWidget::_updateMinMaxFromAlphaBeta() {
    _updating_values = true;
    ContrastOptions options = getOptions();
    options.calculateMinMaxFromAlphaBeta();

    // Update the min/max display values without triggering signals
    ui->display_min_spinbox->setValue(static_cast<int>(std::round(options.display_min)));
    ui->display_max_spinbox->setValue(static_cast<int>(std::round(options.display_max)));

    _updating_values = false;
}


void ContrastWidget::_updateOptions() {
    emit optionsChanged(getOptions());
}

void ContrastWidget::_blockSignalsAndSetValues(ContrastOptions const& options) {
    // Block signals to prevent recursive updates
    ui->active_checkbox->blockSignals(true);
    ui->alpha_spinbox->blockSignals(true);
    ui->beta_spinbox->blockSignals(true);
    ui->display_min_spinbox->blockSignals(true);
    ui->display_max_spinbox->blockSignals(true);

    // Set values
    ui->active_checkbox->setChecked(options.active);
    ui->alpha_spinbox->setValue(options.alpha);
    ui->beta_spinbox->setValue(options.beta);
    ui->display_min_spinbox->setValue(static_cast<int>(std::round(options.display_min)));
    ui->display_max_spinbox->setValue(static_cast<int>(std::round(options.display_max)));

    // Unblock signals
    ui->active_checkbox->blockSignals(false);
    ui->alpha_spinbox->blockSignals(false);
    ui->beta_spinbox->blockSignals(false);
    ui->display_min_spinbox->blockSignals(false);
    ui->display_max_spinbox->blockSignals(false);
}

void ContrastWidget::_updateSpinboxRanges() {
    // Update display_max_spinbox minimum to be at least display_min + 1
    int current_min = ui->display_min_spinbox->value();
    int current_max = ui->display_max_spinbox->value();

    // Set minimum value for max spinbox to be current min + 1
    ui->display_max_spinbox->setMinimum(current_min + 1);

    // Set maximum value for min spinbox to be current max - 1
    ui->display_min_spinbox->setMaximum(current_max - 1);

    // Ensure the current values are still valid
    if (current_max <= current_min) {
        _updating_values = true;
        ui->display_max_spinbox->setValue(current_min + 1);
        _updating_values = false;
    }
}