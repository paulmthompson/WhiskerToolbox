#include "LineKalmanGrouping_Widget.hpp"
#include "ui_LineKalmanGrouping_Widget.h"

#include "DataManager/transforms/Lines/Line_Kalman_Grouping/line_kalman_grouping.hpp"
#include "DataManager/DataManager.hpp"

#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QLabel>
#include <QGroupBox>
#include <QPushButton>

LineKalmanGrouping_Widget::LineKalmanGrouping_Widget(QWidget* parent)
    : DataManagerParameter_Widget(parent)
    , ui(std::make_unique<Ui::LineKalmanGrouping_Widget>())
{
    ui->setupUi(this);
    setupUI();
    connectSignals();
}

LineKalmanGrouping_Widget::~LineKalmanGrouping_Widget() = default;

void LineKalmanGrouping_Widget::setupUI()
{
    // Set reasonable default values
    // Kalman Filter Parameters
    ui->dtSpinBox->setValue(1.0);
    ui->processNoisePositionSpinBox->setValue(10.0);
    ui->processNoiseVelocitySpinBox->setValue(1.0);
    ui->measurementNoiseSpinBox->setValue(5.0);
    ui->initialPositionUncertaintySpinBox->setValue(50.0);
    ui->initialVelocityUncertaintySpinBox->setValue(10.0);
    
    // Assignment Parameters
    ui->maxAssignmentDistanceSpinBox->setValue(100.0);
    ui->minKalmanConfidenceSpinBox->setValue(0.1);
    
    // Algorithm Control
    ui->estimateNoiseEmpiricallyCheckBox->setChecked(true);
    ui->runBackwardSmoothingCheckBox->setChecked(true);
    ui->createNewGroupCheckBox->setChecked(false);
    ui->newGroupNameLineEdit->setText("Kalman Tracked Lines");
    ui->verboseOutputCheckBox->setChecked(false);
    
    // Update control states based on checkboxes
    updateNoiseControls();
    updateNewGroupControls();
}

void LineKalmanGrouping_Widget::connectSignals()
{
    // Kalman Filter Parameters
    connect(ui->dtSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LineKalmanGrouping_Widget::onParametersChanged);
    
    connect(ui->processNoisePositionSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LineKalmanGrouping_Widget::onParametersChanged);
    
    connect(ui->processNoiseVelocitySpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LineKalmanGrouping_Widget::onParametersChanged);
    
    connect(ui->measurementNoiseSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LineKalmanGrouping_Widget::onParametersChanged);
    
    connect(ui->initialPositionUncertaintySpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LineKalmanGrouping_Widget::onParametersChanged);
    
    connect(ui->initialVelocityUncertaintySpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LineKalmanGrouping_Widget::onParametersChanged);
    
    // Assignment Parameters
    connect(ui->maxAssignmentDistanceSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LineKalmanGrouping_Widget::onParametersChanged);
    
    connect(ui->minKalmanConfidenceSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LineKalmanGrouping_Widget::onParametersChanged);
    
    // Algorithm Control checkboxes
    connect(ui->estimateNoiseEmpiricallyCheckBox, &QCheckBox::toggled,
            this, &LineKalmanGrouping_Widget::onEstimateNoiseToggled);
    
    connect(ui->runBackwardSmoothingCheckBox, &QCheckBox::toggled,
            this, &LineKalmanGrouping_Widget::onBackwardSmoothingToggled);
    
    connect(ui->createNewGroupCheckBox, &QCheckBox::toggled,
            this, &LineKalmanGrouping_Widget::onCreateNewGroupToggled);
    
    connect(ui->newGroupNameLineEdit, &QLineEdit::textChanged,
            this, &LineKalmanGrouping_Widget::onParametersChanged);
    
    connect(ui->verboseOutputCheckBox, &QCheckBox::toggled,
            this, &LineKalmanGrouping_Widget::onParametersChanged);
    
    // Reset button
    connect(ui->resetDefaultsButton, &QPushButton::clicked,
            this, &LineKalmanGrouping_Widget::resetToDefaults);
}

std::unique_ptr<TransformParametersBase> LineKalmanGrouping_Widget::getParameters() const
{
    if (!dataManager()) {
        return nullptr;
    }
    
    auto group_manager = dataManager()->getEntityGroupManager();
    if (!group_manager) {
        return nullptr;
    }
    
    auto params = std::make_unique<LineKalmanGroupingParameters>(group_manager);
    
    // Kalman Filter Parameters
    params->dt = ui->dtSpinBox->value();
    params->process_noise_position = ui->processNoisePositionSpinBox->value();
    params->process_noise_velocity = ui->processNoiseVelocitySpinBox->value();
    params->measurement_noise = ui->measurementNoiseSpinBox->value();
    params->initial_position_uncertainty = ui->initialPositionUncertaintySpinBox->value();
    params->initial_velocity_uncertainty = ui->initialVelocityUncertaintySpinBox->value();
    
    // Assignment Parameters
    params->max_assignment_distance = ui->maxAssignmentDistanceSpinBox->value();
    params->min_kalman_confidence = ui->minKalmanConfidenceSpinBox->value();
    
    // Algorithm Control
    params->estimate_noise_empirically = ui->estimateNoiseEmpiricallyCheckBox->isChecked();
    params->run_backward_smoothing = ui->runBackwardSmoothingCheckBox->isChecked();
    params->create_new_group_for_outliers = ui->createNewGroupCheckBox->isChecked();
    params->new_group_name = ui->newGroupNameLineEdit->text().toStdString();
    params->verbose_output = ui->verboseOutputCheckBox->isChecked();
    
    return std::move(params);
}

void LineKalmanGrouping_Widget::onDataManagerChanged()
{
    // Called when the DataManager changes
    // Validate that we have a group manager available
    if (dataManager()) {
        auto group_manager = dataManager()->getEntityGroupManager();
        if (!group_manager) {
            setEnabled(false);
        } else {
            setEnabled(true);
        }
    } else {
        setEnabled(false);
    }
}

void LineKalmanGrouping_Widget::onParametersChanged()
{
    // Validate parameters
    if (validateParameters()) {
        updateParametersFromUI();
    }
}

void LineKalmanGrouping_Widget::onCreateNewGroupToggled([[maybe_unused]] bool enabled)
{
    updateNewGroupControls();
    onParametersChanged();
}

void LineKalmanGrouping_Widget::onEstimateNoiseToggled([[maybe_unused]] bool enabled)
{
    updateNoiseControls();
    onParametersChanged();
}

void LineKalmanGrouping_Widget::onBackwardSmoothingToggled([[maybe_unused]] bool enabled)
{
    onParametersChanged();
}

void LineKalmanGrouping_Widget::updateParametersFromUI()
{
    // This could be used to update any dependent UI elements
    // or emit signals if needed
}

void LineKalmanGrouping_Widget::updateNewGroupControls()
{
    bool enabled = ui->createNewGroupCheckBox->isChecked();
    ui->newGroupNameLineEdit->setEnabled(enabled);
    ui->newGroupNameLabel->setEnabled(enabled);
}

void LineKalmanGrouping_Widget::updateNoiseControls()
{
    bool manual_noise = !ui->estimateNoiseEmpiricallyCheckBox->isChecked();
    
    // Enable/disable manual noise parameter controls
    ui->processNoisePositionSpinBox->setEnabled(manual_noise);
    ui->processNoiseVelocitySpinBox->setEnabled(manual_noise);
    ui->measurementNoiseSpinBox->setEnabled(manual_noise);
    ui->initialPositionUncertaintySpinBox->setEnabled(manual_noise);
    ui->initialVelocityUncertaintySpinBox->setEnabled(manual_noise);
    
    ui->processNoisePositionLabel->setEnabled(manual_noise);
    ui->processNoiseVelocityLabel->setEnabled(manual_noise);
    ui->measurementNoiseLabel->setEnabled(manual_noise);
    ui->initialPositionUncertaintyLabel->setEnabled(manual_noise);
    ui->initialVelocityUncertaintyLabel->setEnabled(manual_noise);
    
    // Show note about empirical estimation
    if (ui->estimateNoiseEmpiricallyCheckBox->isChecked()) {
        ui->noiseEstimationNoteLabel->setText(
            "Noise parameters will be estimated from existing grouped data");
    } else {
        ui->noiseEstimationNoteLabel->setText(
            "Manual noise parameter values will be used");
    }
}

void LineKalmanGrouping_Widget::resetToDefaults()
{
    setupUI();
}

bool LineKalmanGrouping_Widget::validateParameters() const
{
    // Basic validation
    if (ui->dtSpinBox->value() <= 0.0) return false;
    if (ui->maxAssignmentDistanceSpinBox->value() <= 0.0) return false;
    if (ui->minKalmanConfidenceSpinBox->value() < 0.0 || ui->minKalmanConfidenceSpinBox->value() > 1.0) return false;
    
    if (!ui->estimateNoiseEmpiricallyCheckBox->isChecked()) {
        if (ui->processNoisePositionSpinBox->value() <= 0.0) return false;
        if (ui->processNoiseVelocitySpinBox->value() <= 0.0) return false;
        if (ui->measurementNoiseSpinBox->value() <= 0.0) return false;
        if (ui->initialPositionUncertaintySpinBox->value() <= 0.0) return false;
        if (ui->initialVelocityUncertaintySpinBox->value() <= 0.0) return false;
    }
    
    return true;
}