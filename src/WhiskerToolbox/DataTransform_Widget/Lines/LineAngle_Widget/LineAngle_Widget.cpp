#include "LineAngle_Widget.hpp"

#include "ui_LineAngle_Widget.h"

#include "DataManager/transforms/Lines/Line_Angle/line_angle.hpp"

LineAngle_Widget::LineAngle_Widget(QWidget *parent) :
      TransformParameter_Widget(parent),
      ui(new Ui::LineAngle_Widget)
{
    ui->setupUi(this);
    
    // Set default values
    ui->positionSpinBox->setValue(20.0);
    ui->methodComboBox->setCurrentIndex(0); // Direct points by default
    ui->orderSpinBox->setValue(3);          // Default polynomial order
    ui->referenceXSpinBox->setValue(1.0);   // Default reference is positive x-axis (1,0)
    ui->referenceYSpinBox->setValue(0.0);
    
    // Initial stacked widget state
    ui->methodStackedWidget->setCurrentIndex(0);
    
    // Connect signals
    connect(ui->methodComboBox, &QComboBox::currentIndexChanged, 
            ui->methodStackedWidget, &QStackedWidget::setCurrentIndex);
}

LineAngle_Widget::~LineAngle_Widget() {
    delete ui;
}

std::unique_ptr<TransformParametersBase> LineAngle_Widget::getParameters() const {
    auto params = std::make_unique<LineAngleParameters>();
    
    // Get position value (convert from percentage to 0.0-1.0)
    params->position = static_cast<float>(ui->positionSpinBox->value()) / 100.0f;
    
    // Get reference vector
    params->reference_x = static_cast<float>(ui->referenceXSpinBox->value());
    params->reference_y = static_cast<float>(ui->referenceYSpinBox->value());
    
    // Get calculation method
    int methodIndex = ui->methodComboBox->currentIndex();
    if (methodIndex == 0) {
        params->method = AngleCalculationMethod::DirectPoints;
    } else if (methodIndex == 1) {
        params->method = AngleCalculationMethod::PolynomialFit;
        
        // Get polynomial order if method is polynomial fit
        params->polynomial_order = ui->orderSpinBox->value();
    }
    
    return params;
}
