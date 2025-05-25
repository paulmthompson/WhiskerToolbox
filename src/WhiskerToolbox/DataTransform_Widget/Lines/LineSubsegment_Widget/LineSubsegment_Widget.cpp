#include "LineSubsegment_Widget.hpp"
#include "ui_LineSubsegment_Widget.h"

LineSubsegment_Widget::LineSubsegment_Widget(QWidget *parent) :
    TransformParameter_Widget(parent),
    ui(new Ui::LineSubsegment_Widget)
{
    ui->setupUi(this);
    
    setupMethodComboBox();
    
    // Connect signals for real-time validation
    connect(ui->methodComboBox, &QComboBox::currentIndexChanged, this, &LineSubsegment_Widget::onMethodChanged);
    connect(ui->startPositionSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &LineSubsegment_Widget::onParameterChanged);
    connect(ui->endPositionSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &LineSubsegment_Widget::onParameterChanged);
    connect(ui->preserveSpacingCheckBox, &QCheckBox::toggled, this, &LineSubsegment_Widget::onParameterChanged);
    connect(ui->polynomialOrderSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &LineSubsegment_Widget::onParameterChanged);
    connect(ui->outputPointsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &LineSubsegment_Widget::onParameterChanged);
    
    // Initialize UI state
    onMethodChanged(0);
    validateParameters();
}

LineSubsegment_Widget::~LineSubsegment_Widget()
{
    delete ui;
}

void LineSubsegment_Widget::setupMethodComboBox()
{
    ui->methodComboBox->clear();
    ui->methodComboBox->addItem("Direct Extraction", static_cast<int>(SubsegmentExtractionMethod::Direct));
    ui->methodComboBox->addItem("Parametric Interpolation", static_cast<int>(SubsegmentExtractionMethod::Parametric));
    
    ui->methodComboBox->setCurrentIndex(1); // Default to Parametric
}

void LineSubsegment_Widget::onMethodChanged(int index)
{
    if (index < 0) return;
    
    SubsegmentExtractionMethod method = static_cast<SubsegmentExtractionMethod>(
        ui->methodComboBox->itemData(index).toInt()
    );
    
    // Switch to appropriate page in stacked widget
    switch (method) {
        case SubsegmentExtractionMethod::Direct:
            ui->methodStackedWidget->setCurrentWidget(ui->directPage);
            break;
        case SubsegmentExtractionMethod::Parametric:
            ui->methodStackedWidget->setCurrentWidget(ui->parametricPage);
            break;
    }
    
    updateMethodDescription();
    validateParameters();
}

void LineSubsegment_Widget::onParameterChanged()
{
    validateParameters();
}

void LineSubsegment_Widget::updateMethodDescription()
{
    int methodIndex = ui->methodComboBox->currentIndex();
    if (methodIndex < 0) return;
    
    SubsegmentExtractionMethod method = static_cast<SubsegmentExtractionMethod>(
        ui->methodComboBox->itemData(methodIndex).toInt()
    );
    
    QString description;
    switch (method) {
        case SubsegmentExtractionMethod::Direct:
            description = "Direct extraction selects points from the original line based on position indices. Fast and preserves original data characteristics.";
            break;
        case SubsegmentExtractionMethod::Parametric:
            description = "Parametric extraction fits polynomials to the entire line and generates smooth subsegments. Provides better interpolation but requires sufficient input points.";
            break;
    }
    
    ui->methodDescriptionLabel->setText(description);
}

void LineSubsegment_Widget::validateParameters()
{
    QString warning;
    
    // Validate position range
    double startPos = ui->startPositionSpinBox->value();
    double endPos = ui->endPositionSpinBox->value();
    
    if (startPos >= endPos) {
        warning = "Warning: Start position must be less than end position.";
    } else if (endPos - startPos < 1.0) {
        warning = "Warning: Very small subsegment (less than 1% of line length) may not be meaningful.";
    }
    
    // Method-specific validation
    int methodIndex = ui->methodComboBox->currentIndex();
    if (methodIndex >= 0) {
        SubsegmentExtractionMethod method = static_cast<SubsegmentExtractionMethod>(
            ui->methodComboBox->itemData(methodIndex).toInt()
        );
        
        if (method == SubsegmentExtractionMethod::Parametric) {
            int polyOrder = ui->polynomialOrderSpinBox->value();
            int outputPoints = ui->outputPointsSpinBox->value();
            
            if (outputPoints < polyOrder + 1) {
                if (warning.isEmpty()) {
                    warning = QString("Warning: Output points (%1) should be greater than polynomial order (%2) for stable fitting.")
                              .arg(outputPoints).arg(polyOrder);
                }
            }
            
            if (outputPoints < 5) {
                if (warning.isEmpty()) {
                    warning = "Warning: Very few output points may result in poor subsegment quality.";
                }
            }
        }
    }
    
    ui->warningLabel->setText(warning);
}

std::unique_ptr<TransformParametersBase> LineSubsegment_Widget::getParameters() const
{
    auto params = std::make_unique<LineSubsegmentParameters>();
    
    // Position parameters (convert from percentage to fraction)
    params->start_position = static_cast<float>(ui->startPositionSpinBox->value() / 100.0);
    params->end_position = static_cast<float>(ui->endPositionSpinBox->value() / 100.0);
    
    // Method
    int methodIndex = ui->methodComboBox->currentIndex();
    if (methodIndex >= 0) {
        params->method = static_cast<SubsegmentExtractionMethod>(
            ui->methodComboBox->itemData(methodIndex).toInt()
        );
    }
    
    // Method-specific parameters
    switch (params->method) {
        case SubsegmentExtractionMethod::Direct:
            params->preserve_original_spacing = ui->preserveSpacingCheckBox->isChecked();
            break;
        case SubsegmentExtractionMethod::Parametric:
            params->polynomial_order = ui->polynomialOrderSpinBox->value();
            params->output_points = ui->outputPointsSpinBox->value();
            break;
    }
    
    return params;
} 