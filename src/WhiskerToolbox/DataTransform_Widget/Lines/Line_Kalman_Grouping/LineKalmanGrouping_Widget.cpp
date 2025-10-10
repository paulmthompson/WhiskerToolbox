#include "LineKalmanGrouping_Widget.hpp"
#include "ui_LineKalmanGrouping_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/transforms/Lines/Line_Kalman_Grouping/line_kalman_grouping.hpp"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

namespace {
constexpr double kDefaultDt = 1.0;
constexpr double kDefaultProcessNoisePosition = 10.0;
constexpr double kDefaultProcessNoiseVelocity = 1.0;
constexpr double kDefaultStaticNoiseScale = 0.01;
constexpr double kDefaultMeasNoisePosition = 5.0;
constexpr double kDefaultMeasNoiseLength = 10.0;
constexpr double kDefaultInitialPosUncertainty = 50.0;
constexpr double kDefaultInitialVelUncertainty = 10.0;
constexpr double kDefaultCheapLinkageThreshold = 5.0;
constexpr double kDefaultStaticNoisePercentile = 0.75;  // Use 75% of observed variation (was 0.1)
constexpr double kDefaultMinCorrelationThreshold = 0.1;
}// namespace

LineKalmanGrouping_Widget::LineKalmanGrouping_Widget(QWidget * parent)
    : DataManagerParameter_Widget(parent),
      ui(std::make_unique<Ui::LineKalmanGrouping_Widget>()) {
    ui->setupUi(this);
    setupUI();
    connectSignals();
}

LineKalmanGrouping_Widget::~LineKalmanGrouping_Widget() = default;

void LineKalmanGrouping_Widget::setupUI() {
    // Set reasonable default values
    // Kalman Filter Parameters
    ui->dtSpinBox->setValue(kDefaultDt);
    ui->processNoisePositionSpinBox->setValue(kDefaultProcessNoisePosition);
    ui->processNoiseVelocitySpinBox->setValue(kDefaultProcessNoiseVelocity);
    ui->staticNoiseScaleSpinBox->setValue(kDefaultStaticNoiseScale);
    ui->measurementNoisePositionSpinBox->setValue(kDefaultMeasNoisePosition);
    ui->measurementNoiseLengthSpinBox->setValue(kDefaultMeasNoiseLength);
    ui->initialPositionUncertaintySpinBox->setValue(kDefaultInitialPosUncertainty);
    ui->initialVelocityUncertaintySpinBox->setValue(kDefaultInitialVelUncertainty);

    // Assignment Parameters
    ui->cheapLinkageThresholdSpinBox->setValue(kDefaultCheapLinkageThreshold);

    // Auto-Estimation Parameters
    ui->autoEstimateStaticNoiseCheckBox->setChecked(false);
    ui->autoEstimateMeasurementNoiseCheckBox->setChecked(false);
    ui->staticNoisePercentileSpinBox->setValue(kDefaultStaticNoisePercentile);

    // Cross-Covariance Parameters
    ui->enableCrossCovarianceCheckBox->setChecked(false);
    ui->minCorrelationThresholdSpinBox->setValue(kDefaultMinCorrelationThreshold);

    // Algorithm Control
    ui->verboseOutputCheckBox->setChecked(false);
    ui->writePutativeGroupsCheckBox->setChecked(true);
    ui->putativePrefixLineEdit->setText("Putative:");
}

void LineKalmanGrouping_Widget::connectSignals() {
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
    connect(ui->cheapLinkageThresholdSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LineKalmanGrouping_Widget::onParametersChanged);

    // Auto-Estimation Parameters
    connect(ui->autoEstimateStaticNoiseCheckBox, &QCheckBox::toggled,
            this, &LineKalmanGrouping_Widget::onParametersChanged);

    connect(ui->autoEstimateMeasurementNoiseCheckBox, &QCheckBox::toggled,
            this, &LineKalmanGrouping_Widget::onParametersChanged);

    connect(ui->staticNoisePercentileSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LineKalmanGrouping_Widget::onParametersChanged);

    // Cross-Covariance Parameters
    connect(ui->enableCrossCovarianceCheckBox, &QCheckBox::toggled,
            this, &LineKalmanGrouping_Widget::onParametersChanged);

    connect(ui->minCorrelationThresholdSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LineKalmanGrouping_Widget::onParametersChanged);

    // Algorithm Control checkboxes
    connect(ui->verboseOutputCheckBox, &QCheckBox::toggled,
            this, &LineKalmanGrouping_Widget::onParametersChanged);
    connect(ui->writePutativeGroupsCheckBox, &QCheckBox::toggled,
            this, &LineKalmanGrouping_Widget::onParametersChanged);
    connect(ui->putativePrefixLineEdit, &QLineEdit::textChanged,
            this, &LineKalmanGrouping_Widget::onParametersChanged);

    // Reset button
    connect(ui->resetDefaultsButton, &QPushButton::clicked,
            this, &LineKalmanGrouping_Widget::resetToDefaults);
}

std::unique_ptr<TransformParametersBase> LineKalmanGrouping_Widget::getParameters() const {
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

    // Auto-Estimation Parameters
    params->auto_estimate_static_noise = ui->autoEstimateStaticNoiseCheckBox->isChecked();
    params->auto_estimate_measurement_noise = ui->autoEstimateMeasurementNoiseCheckBox->isChecked();
    params->static_noise_percentile = ui->staticNoisePercentileSpinBox->value();

    // Cross-Covariance Parameters
    params->enable_cross_feature_covariance = ui->enableCrossCovarianceCheckBox->isChecked();
    params->min_correlation_threshold = ui->minCorrelationThresholdSpinBox->value();

    // Algorithm Control
    params->verbose_output = ui->verboseOutputCheckBox->isChecked();
    params->cheap_assignment_threshold = ui->cheapLinkageThresholdSpinBox->value();
    params->write_to_putative_groups = ui->writePutativeGroupsCheckBox->isChecked();
    params->putative_group_prefix = ui->putativePrefixLineEdit->text().toStdString();

    return params;
}

void LineKalmanGrouping_Widget::onDataManagerChanged() {
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

void LineKalmanGrouping_Widget::onParametersChanged() {
    // Validate parameters
    if (validateParameters()) {
        updateParametersFromUI();
    }
}

void LineKalmanGrouping_Widget::updateParametersFromUI() {
    // This could be used to update any dependent UI elements
    // or emit signals if needed
}

void LineKalmanGrouping_Widget::resetToDefaults() {
    setupUI();
}

bool LineKalmanGrouping_Widget::validateParameters() const {
    // Basic validation
    if (ui->dtSpinBox->value() <= 0.0) return false;
    if (ui->cheapLinkageThresholdSpinBox->value() <= 0.0) return false;
    if (ui->processNoisePositionSpinBox->value() <= 0.0) return false;
    if (ui->processNoiseVelocitySpinBox->value() <= 0.0) return false;
    if (ui->staticNoiseScaleSpinBox->value() <= 0.0) return false;
    if (ui->measurementNoisePositionSpinBox->value() <= 0.0) return false;
    if (ui->measurementNoiseLengthSpinBox->value() <= 0.0) return false;
    if (ui->initialPositionUncertaintySpinBox->value() <= 0.0) return false;
    if (ui->initialVelocityUncertaintySpinBox->value() <= 0.0) return false;
    if (ui->staticNoisePercentileSpinBox->value() <= 0.0) return false;
    if (ui->staticNoisePercentileSpinBox->value() > 1.0) return false;
    if (ui->minCorrelationThresholdSpinBox->value() < 0.0) return false;
    if (ui->minCorrelationThresholdSpinBox->value() > 1.0) return false;

    return true;
}