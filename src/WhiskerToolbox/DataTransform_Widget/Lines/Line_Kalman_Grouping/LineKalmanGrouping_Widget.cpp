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
    ui->staticNoiseScaleSpinBox->setValue(0.01);
    ui->measurementNoisePositionSpinBox->setValue(5.0);
    ui->measurementNoiseLengthSpinBox->setValue(10.0);
    ui->initialPositionUncertaintySpinBox->setValue(50.0);
    ui->initialVelocityUncertaintySpinBox->setValue(10.0);
    
    // Assignment Parameters
    ui->maxAssignmentDistanceSpinBox->setValue(3.0);
    
    // Auto-Estimation Parameters
    ui->autoEstimateStaticNoiseCheckBox->setChecked(false);
    ui->autoEstimateMeasurementNoiseCheckBox->setChecked(false);
    ui->staticNoisePercentileSpinBox->setValue(0.1);
    
    // Algorithm Control
    ui->verboseOutputCheckBox->setChecked(false);
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
    
    connect(ui->staticNoiseScaleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LineKalmanGrouping_Widget::onParametersChanged);
    
    connect(ui->measurementNoisePositionSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LineKalmanGrouping_Widget::onParametersChanged);
    
    connect(ui->measurementNoiseLengthSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LineKalmanGrouping_Widget::onParametersChanged);
    
    connect(ui->initialPositionUncertaintySpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LineKalmanGrouping_Widget::onParametersChanged);
    
    connect(ui->initialVelocityUncertaintySpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LineKalmanGrouping_Widget::onParametersChanged);
    
    // Assignment Parameters
    connect(ui->maxAssignmentDistanceSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LineKalmanGrouping_Widget::onParametersChanged);
    
    // Auto-Estimation Parameters
    connect(ui->autoEstimateStaticNoiseCheckBox, &QCheckBox::toggled,
            this, &LineKalmanGrouping_Widget::onParametersChanged);
    
    connect(ui->autoEstimateMeasurementNoiseCheckBox, &QCheckBox::toggled,
            this, &LineKalmanGrouping_Widget::onParametersChanged);
    
    connect(ui->staticNoisePercentileSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LineKalmanGrouping_Widget::onParametersChanged);
    
    // Algorithm Control checkboxes
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
    params->static_feature_process_noise_scale = ui->staticNoiseScaleSpinBox->value();
    params->measurement_noise_position = ui->measurementNoisePositionSpinBox->value();
    params->measurement_noise_length = ui->measurementNoiseLengthSpinBox->value();
    params->initial_position_uncertainty = ui->initialPositionUncertaintySpinBox->value();
    params->initial_velocity_uncertainty = ui->initialVelocityUncertaintySpinBox->value();
    
    // Assignment Parameters
    params->max_assignment_distance = ui->maxAssignmentDistanceSpinBox->value();
    
    // Auto-Estimation Parameters
    params->auto_estimate_static_noise = ui->autoEstimateStaticNoiseCheckBox->isChecked();
    params->auto_estimate_measurement_noise = ui->autoEstimateMeasurementNoiseCheckBox->isChecked();
    params->static_noise_percentile = ui->staticNoisePercentileSpinBox->value();
    
    // Algorithm Control
    params->verbose_output = ui->verboseOutputCheckBox->isChecked();
    
    return params;
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

void LineKalmanGrouping_Widget::updateParametersFromUI()
{
    // This could be used to update any dependent UI elements
    // or emit signals if needed
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
    if (ui->processNoisePositionSpinBox->value() <= 0.0) return false;
    if (ui->processNoiseVelocitySpinBox->value() <= 0.0) return false;
    if (ui->staticNoiseScaleSpinBox->value() <= 0.0) return false;
    if (ui->measurementNoisePositionSpinBox->value() <= 0.0) return false;
    if (ui->measurementNoiseLengthSpinBox->value() <= 0.0) return false;
    if (ui->initialPositionUncertaintySpinBox->value() <= 0.0) return false;
    if (ui->initialVelocityUncertaintySpinBox->value() <= 0.0) return false;
    if (ui->staticNoisePercentileSpinBox->value() <= 0.0) return false;
    if (ui->staticNoisePercentileSpinBox->value() > 1.0) return false;
    
    return true;
}