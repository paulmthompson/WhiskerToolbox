#include "LineCurvature_Widget.hpp"
#include "ui_LineCurvature_Widget.h"

#include "DataManager/transforms/Lines/line_curvature.hpp"

LineCurvature_Widget::LineCurvature_Widget(QWidget *parent) :
    TransformParameter_Widget(parent),
    ui(new Ui::LineCurvature_Widget)
{
    ui->setupUi(this);

    // Set default values from LineCurvatureParameters
    LineCurvatureParameters default_params;
    ui->positionSpinBox->setValue(static_cast<double>(default_params.position * 100.0f)); // Convert to percentage
    ui->polynomialOrderSpinBox->setValue(default_params.polynomial_order);
    ui->fittingWindowSpinBox->setValue(static_cast<double>(default_params.fitting_window_percentage * 100.0f)); // Convert to percentage

    // Setup method combobox
    // Currently only PolynomialFit is an option
    ui->methodComboBox->addItem("Polynomial Fit", static_cast<int>(CurvatureCalculationMethod::PolynomialFit));
    ui->methodComboBox->setCurrentIndex(0); // Default to Polynomial Fit

    // Connect signals for method changes (though only one method for now)
    // This will make it easier to add more methods later
    connect(ui->methodComboBox, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &LineCurvature_Widget::onMethodChanged);

    // Initialize the stacked widget to show the polynomial page
    onMethodChanged(0); // Call initially to set the correct page
}

LineCurvature_Widget::~LineCurvature_Widget()
{
    delete ui;
}

void LineCurvature_Widget::onMethodChanged(int index)
{
    // The user data in the combo box stores the enum value
    auto method_enum_val = static_cast<CurvatureCalculationMethod>(ui->methodComboBox->itemData(index).toInt());

    if (method_enum_val == CurvatureCalculationMethod::PolynomialFit) {
        ui->methodStackedWidget->setCurrentWidget(ui->polynomialPage);
    }
    // Add else-if blocks here for other methods if they are added
    // else if (method_enum_val == CurvatureCalculationMethod::AnotherMethod) {
    //    ui->methodStackedWidget->setCurrentWidget(ui->anotherMethodPage);
    // }
}

std::unique_ptr<TransformParametersBase> LineCurvature_Widget::getParameters() const
{
    auto params = std::make_unique<LineCurvatureParameters>();

    params->position = static_cast<float>(ui->positionSpinBox->value() / 100.0); // Convert percentage to 0.0-1.0
    
    int method_idx = ui->methodComboBox->currentIndex();
    params->method = static_cast<CurvatureCalculationMethod>(ui->methodComboBox->itemData(method_idx).toInt());

    if (params->method == CurvatureCalculationMethod::PolynomialFit) {
        params->polynomial_order = ui->polynomialOrderSpinBox->value();
        params->fitting_window_percentage = static_cast<float>(ui->fittingWindowSpinBox->value() / 100.0); // Convert to 0.0-1.0
    }
    // Add else-if blocks for other methods if needed

    return params;
} 