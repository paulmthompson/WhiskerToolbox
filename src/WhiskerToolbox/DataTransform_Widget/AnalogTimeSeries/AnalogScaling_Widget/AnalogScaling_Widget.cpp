#include "AnalogScaling_Widget.hpp"
#include "ui_AnalogScaling_Widget.h"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DataManager.hpp"

#include <QLocale>

AnalogScaling_Widget::AnalogScaling_Widget(QWidget *parent) :
    TransformParameter_Widget(parent),
    ui(new Ui::AnalogScaling_Widget),
    _data_manager(nullptr)
{
    ui->setupUi(this);
    
    setupMethodComboBox();
    
    // Connect signals for real-time updates
    connect(ui->methodComboBox, &QComboBox::currentIndexChanged, this, &AnalogScaling_Widget::onMethodChanged);
    connect(ui->gainFactorSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &AnalogScaling_Widget::onParameterChanged);
    connect(ui->stdDevTargetSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &AnalogScaling_Widget::onParameterChanged);
    connect(ui->minTargetSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &AnalogScaling_Widget::onParameterChanged);
    connect(ui->maxTargetSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &AnalogScaling_Widget::onParameterChanged);
    
    // Initialize UI state
    updateParameterVisibility();
    updateMethodDescription();
}

AnalogScaling_Widget::~AnalogScaling_Widget()
{
    delete ui;
}

void AnalogScaling_Widget::setDataManager(std::shared_ptr<DataManager> data_manager)
{
    _data_manager = std::move(data_manager);
}

void AnalogScaling_Widget::setCurrentDataKey(QString const & data_key)
{
    _current_data_key = data_key;
    updateStatistics();
}

void AnalogScaling_Widget::setupMethodComboBox()
{
    ui->methodComboBox->clear();
    ui->methodComboBox->addItem("Fixed Gain", static_cast<int>(ScalingMethod::FixedGain));
    ui->methodComboBox->addItem("Z-Score Normalization", static_cast<int>(ScalingMethod::ZScore));
    ui->methodComboBox->addItem("Standard Deviation Scaling", static_cast<int>(ScalingMethod::StandardDeviation));
    ui->methodComboBox->addItem("Min-Max Normalization", static_cast<int>(ScalingMethod::MinMax));
    ui->methodComboBox->addItem("Robust Scaling (IQR)", static_cast<int>(ScalingMethod::RobustScaling));
    ui->methodComboBox->addItem("Unit Variance Scaling", static_cast<int>(ScalingMethod::UnitVariance));
    ui->methodComboBox->addItem("Centering (Zero Mean)", static_cast<int>(ScalingMethod::Centering));
    
    ui->methodComboBox->setCurrentIndex(1); // Default to Z-Score
}

void AnalogScaling_Widget::onMethodChanged(int index)
{
    updateParameterVisibility();
    updateMethodDescription();
    validateParameters();
}

void AnalogScaling_Widget::onParameterChanged()
{
    validateParameters();
}

void AnalogScaling_Widget::updateStatistics()
{
    if (!_data_manager || _current_data_key.isEmpty()) {
        // Clear statistics display
        ui->meanValueLabel->setText("-");
        ui->stdDevValueLabel->setText("-");
        ui->minValueLabel->setText("-");
        ui->maxValueLabel->setText("-");
        ui->medianValueLabel->setText("-");
        ui->iqrValueLabel->setText("-");
        ui->sampleCountValueLabel->setText("-");
        return;
    }
    
    auto analog_data = _data_manager->getData<AnalogTimeSeries>(_current_data_key.toStdString());
    if (!analog_data) {
        return;
    }
    
    _current_stats = calculate_analog_statistics(analog_data.get());
    displayStatistics(_current_stats);
}

void AnalogScaling_Widget::updateParameterVisibility()
{
    int methodIndex = ui->methodComboBox->currentIndex();
    if (methodIndex < 0) return;
    
    ScalingMethod method = static_cast<ScalingMethod>(ui->methodComboBox->itemData(methodIndex).toInt());
    
    // Hide all parameters first
    ui->gainFactorLabel->setVisible(false);
    ui->gainFactorSpinBox->setVisible(false);
    ui->stdDevTargetLabel->setVisible(false);
    ui->stdDevTargetSpinBox->setVisible(false);
    ui->minTargetLabel->setVisible(false);
    ui->minTargetSpinBox->setVisible(false);
    ui->maxTargetLabel->setVisible(false);
    ui->maxTargetSpinBox->setVisible(false);
    
    // Show relevant parameters based on method
    switch (method) {
        case ScalingMethod::FixedGain:
            ui->gainFactorLabel->setVisible(true);
            ui->gainFactorSpinBox->setVisible(true);
            break;
            
        case ScalingMethod::StandardDeviation:
            ui->stdDevTargetLabel->setVisible(true);
            ui->stdDevTargetSpinBox->setVisible(true);
            break;
            
        case ScalingMethod::MinMax:
            ui->minTargetLabel->setVisible(true);
            ui->minTargetSpinBox->setVisible(true);
            ui->maxTargetLabel->setVisible(true);
            ui->maxTargetSpinBox->setVisible(true);
            break;
            
        default:
            // No additional parameters needed
            break;
    }
}

void AnalogScaling_Widget::updateMethodDescription()
{
    int methodIndex = ui->methodComboBox->currentIndex();
    if (methodIndex < 0) return;
    
    ScalingMethod method = static_cast<ScalingMethod>(ui->methodComboBox->itemData(methodIndex).toInt());
    
    QString description;
    switch (method) {
        case ScalingMethod::FixedGain:
            description = "Multiply all values by a constant gain factor.";
            break;
        case ScalingMethod::ZScore:
            description = "Standardize data: (x - mean) / std_dev. Results in zero mean and unit variance.";
            break;
        case ScalingMethod::StandardDeviation:
            description = "Scale so that the specified number of standard deviations equals 1.0.";
            break;
        case ScalingMethod::MinMax:
            description = "Scale data to fit within the specified min-max range.";
            break;
        case ScalingMethod::RobustScaling:
            description = "Scale using median and IQR: (x - median) / IQR. Robust to outliers.";
            break;
        case ScalingMethod::UnitVariance:
            description = "Scale to unit variance (std_dev = 1) without centering.";
            break;
        case ScalingMethod::Centering:
            description = "Subtract the mean to center data around zero.";
            break;
    }
    
    ui->methodDescriptionLabel->setText(description);
}

void AnalogScaling_Widget::validateParameters()
{
    QString warning;
    
    int methodIndex = ui->methodComboBox->currentIndex();
    if (methodIndex < 0) return;
    
    ScalingMethod method = static_cast<ScalingMethod>(ui->methodComboBox->itemData(methodIndex).toInt());
    
    switch (method) {
        case ScalingMethod::FixedGain:
            if (qAbs(ui->gainFactorSpinBox->value()) < 1e-10) {
                warning = "Warning: Gain factor is very close to zero.";
            }
            break;
            
        case ScalingMethod::ZScore:
        case ScalingMethod::StandardDeviation:
        case ScalingMethod::UnitVariance:
            if (_current_stats.std_dev < 1e-10) {
                warning = "Warning: Data has very low standard deviation. Scaling may produce extreme values.";
            }
            break;
            
        case ScalingMethod::MinMax:
            if (ui->maxTargetSpinBox->value() <= ui->minTargetSpinBox->value()) {
                warning = "Warning: Max target must be greater than min target.";
            }
            if (_current_stats.max_val - _current_stats.min_val < 1e-10) {
                warning = "Warning: Data has very small range. Scaling may not be effective.";
            }
            break;
            
        case ScalingMethod::RobustScaling:
            if (_current_stats.iqr < 1e-10) {
                warning = "Warning: Data has very small interquartile range. Scaling may produce extreme values.";
            }
            break;
            
        default:
            break;
    }
    
    ui->warningLabel->setText(warning);
}

void AnalogScaling_Widget::displayStatistics(AnalogStatistics const & stats)
{
    ui->meanValueLabel->setText(formatNumber(stats.mean));
    ui->stdDevValueLabel->setText(formatNumber(stats.std_dev));
    ui->minValueLabel->setText(formatNumber(stats.min_val));
    ui->maxValueLabel->setText(formatNumber(stats.max_val));
    ui->medianValueLabel->setText(formatNumber(stats.median));
    ui->iqrValueLabel->setText(formatNumber(stats.iqr));
    ui->sampleCountValueLabel->setText(QString::number(stats.sample_count));
    
    // Trigger validation after updating statistics
    validateParameters();
}

QString AnalogScaling_Widget::formatNumber(double value, int precision) const
{
    QLocale locale;
    
    // Use scientific notation for very large or very small numbers
    if (qAbs(value) >= 1e6 || (qAbs(value) < 1e-3 && value != 0.0)) {
        return QString::number(value, 'e', precision);
    } else {
        return locale.toString(value, 'f', precision);
    }
}

std::unique_ptr<TransformParametersBase> AnalogScaling_Widget::getParameters() const
{
    auto params = std::make_unique<AnalogScalingParams>();
    
    int methodIndex = ui->methodComboBox->currentIndex();
    if (methodIndex >= 0) {
        params->method = static_cast<ScalingMethod>(ui->methodComboBox->itemData(methodIndex).toInt());
    }
    
    // Set parameters based on method
    params->gain_factor = ui->gainFactorSpinBox->value();
    params->std_dev_target = ui->stdDevTargetSpinBox->value();
    params->min_target = ui->minTargetSpinBox->value();
    params->max_target = ui->maxTargetSpinBox->value();
    
    return params;
} 
