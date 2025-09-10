#include "LinePointExtraction_Widget.hpp"
#include "ui_LinePointExtraction_Widget.h"

#include "DataManager/transforms/Lines/Line_Point_Extraction/line_point_extraction.hpp"

LinePointExtraction_Widget::LinePointExtraction_Widget(QWidget *parent) :
    TransformParameter_Widget(parent),
    ui(new Ui::LinePointExtraction_Widget)
{
    ui->setupUi(this);
    
    setupMethodComboBox();
    
    // Connect signals for real-time validation
    connect(ui->methodComboBox, &QComboBox::currentIndexChanged, this, &LinePointExtraction_Widget::onMethodChanged);
    connect(ui->positionSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &LinePointExtraction_Widget::onParameterChanged);
    connect(ui->useInterpolationCheckBox, &QCheckBox::toggled, this, &LinePointExtraction_Widget::onParameterChanged);
    connect(ui->polynomialOrderSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &LinePointExtraction_Widget::onParameterChanged);
    
    // Initialize UI state
    onMethodChanged(0);
    validateParameters();
}

LinePointExtraction_Widget::~LinePointExtraction_Widget()
{
    delete ui;
}

void LinePointExtraction_Widget::setupMethodComboBox()
{
    ui->methodComboBox->clear();
    ui->methodComboBox->addItem("Direct Extraction", static_cast<int>(PointExtractionMethod::Direct));
    ui->methodComboBox->addItem("Parametric Interpolation", static_cast<int>(PointExtractionMethod::Parametric));
    
    ui->methodComboBox->setCurrentIndex(1); // Default to Parametric
}

void LinePointExtraction_Widget::onMethodChanged(int index)
{
    if (index < 0) return;
    
    PointExtractionMethod method = static_cast<PointExtractionMethod>(
        ui->methodComboBox->itemData(index).toInt()
    );
    
    // Switch to appropriate page in stacked widget
    switch (method) {
        case PointExtractionMethod::Direct:
            ui->methodStackedWidget->setCurrentWidget(ui->directPage);
            break;
        case PointExtractionMethod::Parametric:
            ui->methodStackedWidget->setCurrentWidget(ui->parametricPage);
            break;
    }
    
    updateMethodDescription();
    validateParameters();
}

void LinePointExtraction_Widget::onParameterChanged()
{
    validateParameters();
}

void LinePointExtraction_Widget::updateMethodDescription()
{
    int methodIndex = ui->methodComboBox->currentIndex();
    if (methodIndex < 0) return;
    
    PointExtractionMethod method = static_cast<PointExtractionMethod>(
        ui->methodComboBox->itemData(methodIndex).toInt()
    );
    
    QString description;
    switch (method) {
        case PointExtractionMethod::Direct:
            description = "Direct extraction selects a point from the original line based on cumulative distance. Fast and preserves original data characteristics.";
            break;
        case PointExtractionMethod::Parametric:
            description = "Parametric extraction fits polynomials to the entire line using distance-based parameterization and calculates the exact point at the specified position. Provides smooth interpolation and higher accuracy.";
            break;
    }
    
    ui->methodDescriptionLabel->setText(description);
}

void LinePointExtraction_Widget::validateParameters()
{
    QString warning;
    
    // Validate position
    double position = ui->positionSpinBox->value();
    
    if (position < 0.0 || position > 100.0) {
        warning = "Warning: Position must be between 0% and 100%.";
    }
    
    // Method-specific validation
    int methodIndex = ui->methodComboBox->currentIndex();
    if (methodIndex >= 0) {
        PointExtractionMethod method = static_cast<PointExtractionMethod>(
            ui->methodComboBox->itemData(methodIndex).toInt()
        );
        
        if (method == PointExtractionMethod::Parametric) {
            int polyOrder = ui->polynomialOrderSpinBox->value();
            
            if (polyOrder < 2) {
                if (warning.isEmpty()) {
                    warning = "Warning: Polynomial order should be at least 2 for meaningful fitting.";
                }
            }
            
            if (polyOrder > 6) {
                if (warning.isEmpty()) {
                    warning = "Warning: Very high polynomial orders may cause overfitting or numerical instability.";
                }
            }
        }
    }
    
    ui->warningLabel->setText(warning);
}

std::unique_ptr<TransformParametersBase> LinePointExtraction_Widget::getParameters() const
{
    auto params = std::make_unique<LinePointExtractionParameters>();
    
    // Position parameter (convert from percentage to fraction)
    params->position = static_cast<float>(ui->positionSpinBox->value() / 100.0);
    
    // Method
    int methodIndex = ui->methodComboBox->currentIndex();
    if (methodIndex >= 0) {
        params->method = static_cast<PointExtractionMethod>(
            ui->methodComboBox->itemData(methodIndex).toInt()
        );
    }
    
    // Method-specific parameters
    switch (params->method) {
        case PointExtractionMethod::Direct:
            params->use_interpolation = ui->useInterpolationCheckBox->isChecked();
            break;
        case PointExtractionMethod::Parametric:
            params->polynomial_order = ui->polynomialOrderSpinBox->value();
            break;
    }
    
    return params;
} 