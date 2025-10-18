#include "LineOutlierDetection_Widget.hpp"
#include "ui_LineOutlierDetection_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/transforms/Lines/Line_Outlier_Detection/line_outlier_detection.hpp"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

namespace {
constexpr double kDefaultDt = 1.0;
constexpr double kDefaultProcessNoisePosition = 10.0;
constexpr double kDefaultProcessNoiseVelocity = 1.0;
constexpr double kDefaultProcessNoiseLength = 0.1;
constexpr double kDefaultMeasNoisePosition = 5.0;
constexpr double kDefaultMeasNoiseLength = 10.0;
constexpr double kDefaultInitialPosUncertainty = 50.0;
constexpr double kDefaultInitialVelUncertainty = 10.0;
constexpr double kDefaultInitialLengthUncertainty = 20.0;
constexpr double kDefaultMadThreshold = 5.0;
}// namespace

LineOutlierDetection_Widget::LineOutlierDetection_Widget(QWidget * parent)
    : DataManagerParameter_Widget(parent),
      ui(std::make_unique<Ui::LineOutlierDetection_Widget>()) {
    ui->setupUi(this);
    setupUI();
    connectSignals();
}

LineOutlierDetection_Widget::~LineOutlierDetection_Widget() = default;

void LineOutlierDetection_Widget::setupUI() {
    loadDefaultValues();
}

void LineOutlierDetection_Widget::loadDefaultValues() {
    // Kalman filter parameters
    ui->dtSpinBox->setValue(kDefaultDt);
    ui->processNoisePositionSpinBox->setValue(kDefaultProcessNoisePosition);
    ui->processNoiseVelocitySpinBox->setValue(kDefaultProcessNoiseVelocity);
    ui->processNoiseLengthSpinBox->setValue(kDefaultProcessNoiseLength);
    ui->measurementNoisePositionSpinBox->setValue(kDefaultMeasNoisePosition);
    ui->measurementNoiseLengthSpinBox->setValue(kDefaultMeasNoiseLength);
    ui->initialPositionUncertaintySpinBox->setValue(kDefaultInitialPosUncertainty);
    ui->initialVelocityUncertaintySpinBox->setValue(kDefaultInitialVelUncertainty);
    ui->initialLengthUncertaintySpinBox->setValue(kDefaultInitialLengthUncertainty);
    
    // Outlier detection parameters
    ui->madThresholdSpinBox->setValue(kDefaultMadThreshold);
    
    // Output control
    ui->outlierGroupNameLineEdit->setText("Outliers");
    ui->verboseOutputCheckBox->setChecked(false);
}

void LineOutlierDetection_Widget::connectSignals() {
    // Kalman Filter Parameters
    connect(ui->dtSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LineOutlierDetection_Widget::onParametersChanged);

    connect(ui->processNoisePositionSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LineOutlierDetection_Widget::onParametersChanged);

    connect(ui->processNoiseVelocitySpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LineOutlierDetection_Widget::onParametersChanged);

    connect(ui->processNoiseLengthSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LineOutlierDetection_Widget::onParametersChanged);

    connect(ui->measurementNoisePositionSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LineOutlierDetection_Widget::onParametersChanged);

    connect(ui->measurementNoiseLengthSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LineOutlierDetection_Widget::onParametersChanged);

    connect(ui->initialPositionUncertaintySpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LineOutlierDetection_Widget::onParametersChanged);

    connect(ui->initialVelocityUncertaintySpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LineOutlierDetection_Widget::onParametersChanged);

    connect(ui->initialLengthUncertaintySpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LineOutlierDetection_Widget::onParametersChanged);

    // Outlier Detection Parameters
    connect(ui->madThresholdSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LineOutlierDetection_Widget::onParametersChanged);

    // Output Control
    connect(ui->outlierGroupNameLineEdit, &QLineEdit::textChanged,
            this, &LineOutlierDetection_Widget::onParametersChanged);

    connect(ui->verboseOutputCheckBox, &QCheckBox::toggled,
            this, &LineOutlierDetection_Widget::onParametersChanged);

    // Reset button
    connect(ui->resetDefaultsButton, &QPushButton::clicked,
            this, &LineOutlierDetection_Widget::resetToDefaults);
}

std::unique_ptr<TransformParametersBase> LineOutlierDetection_Widget::getParameters() const {
    if (!dataManager()) {
        return nullptr;
    }

    auto group_manager = dataManager()->getEntityGroupManager();
    if (!group_manager) {
        return nullptr;
    }

    auto params = std::make_unique<LineOutlierDetectionParameters>(group_manager);

    // Kalman filter parameters
    params->dt = ui->dtSpinBox->value();
    params->process_noise_position = ui->processNoisePositionSpinBox->value();
    params->process_noise_velocity = ui->processNoiseVelocitySpinBox->value();
    params->process_noise_length = ui->processNoiseLengthSpinBox->value();
    params->measurement_noise_position = ui->measurementNoisePositionSpinBox->value();
    params->measurement_noise_length = ui->measurementNoiseLengthSpinBox->value();
    params->initial_position_uncertainty = ui->initialPositionUncertaintySpinBox->value();
    params->initial_velocity_uncertainty = ui->initialVelocityUncertaintySpinBox->value();
    params->initial_length_uncertainty = ui->initialLengthUncertaintySpinBox->value();

    // Outlier detection parameters
    params->mad_threshold = ui->madThresholdSpinBox->value();

    // Output control
    params->outlier_group_name = ui->outlierGroupNameLineEdit->text().toStdString();
    params->verbose_output = ui->verboseOutputCheckBox->isChecked();

    return params;
}

void LineOutlierDetection_Widget::onDataManagerChanged() {
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

void LineOutlierDetection_Widget::onParametersChanged() {
    // Validate parameters
    if (validateParameters()) {
        updateParametersFromUI();
    }
}

void LineOutlierDetection_Widget::updateParametersFromUI() {
    // This could be used to update any dependent UI elements
    // or emit signals if needed
}

void LineOutlierDetection_Widget::resetToDefaults() {
    setupUI();
}

bool LineOutlierDetection_Widget::validateParameters() const {
    // Basic validation
    if (ui->dtSpinBox->value() <= 0.0) return false;
    if (ui->processNoisePositionSpinBox->value() <= 0.0) return false;
    if (ui->processNoiseVelocitySpinBox->value() <= 0.0) return false;
    if (ui->processNoiseLengthSpinBox->value() <= 0.0) return false;
    if (ui->measurementNoisePositionSpinBox->value() <= 0.0) return false;
    if (ui->measurementNoiseLengthSpinBox->value() <= 0.0) return false;
    if (ui->initialPositionUncertaintySpinBox->value() <= 0.0) return false;
    if (ui->initialVelocityUncertaintySpinBox->value() <= 0.0) return false;
    if (ui->initialLengthUncertaintySpinBox->value() <= 0.0) return false;
    if (ui->madThresholdSpinBox->value() <= 0.0) return false;

    return true;
}
