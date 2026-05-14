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
    ui->windowSpinBox->setValue(20.0);
    ui->methodComboBox->setCurrentIndex(0);// Direct points by default
    ui->orderSpinBox->setValue(3);// Default polynomial order
    ui->axisPositiveXXSpinBox->setValue(1.0);
    ui->axisPositiveXYSpinBox->setValue(0.0);
    ui->axisPositiveYXSpinBox->setValue(0.0);
    ui->axisPositiveYYSpinBox->setValue(1.0);

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

    params->position = static_cast<float>(ui->positionSpinBox->value()) / 100.0f;
    params->window = static_cast<float>(ui->windowSpinBox->value()) / 100.0f;

    params->axis_x_x = static_cast<float>(ui->axisPositiveXXSpinBox->value());
    params->axis_x_y = static_cast<float>(ui->axisPositiveXYSpinBox->value());
    params->axis_y_x = static_cast<float>(ui->axisPositiveYXSpinBox->value());
    params->axis_y_y = static_cast<float>(ui->axisPositiveYYSpinBox->value());

    int methodIndex = ui->methodComboBox->currentIndex();
    if (methodIndex == 0) {
        params->method = AngleCalculationMethod::DirectPoints;
    } else if (methodIndex == 1) {
        params->method = AngleCalculationMethod::PolynomialFit;

        params->polynomial_order = ui->orderSpinBox->value();
    }

    return params;
}
