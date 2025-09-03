#include "LineResample_Widget.hpp"
#include "ui_LineResample_Widget.h"// Generated from .ui file

#include "DataManager/transforms/Lines/Line_Resample/line_resample.hpp"

LineResample_Widget::LineResample_Widget(QWidget * parent)
    : TransformParameter_Widget(parent),
      ui(new Ui::LineResample_Widget) {
    ui->setupUi(this);

    // Connect the algorithm selection signal
    connect(ui->algorithmComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LineResample_Widget::onAlgorithmChanged);

    // Initialize the UI state
    updateParameterVisibility();
    updateDescription();
}

LineResample_Widget::~LineResample_Widget() {
    delete ui;
}

void LineResample_Widget::onAlgorithmChanged() {
    updateParameterVisibility();
    updateDescription();
}

void LineResample_Widget::updateParameterVisibility() {
    bool isFixedSpacing = (ui->algorithmComboBox->currentIndex() == 0);

    // Show/hide appropriate parameters
    ui->spacingLabel->setVisible(isFixedSpacing);
    ui->targetSpacingSpinBox->setVisible(isFixedSpacing);
    ui->epsilonLabel->setVisible(!isFixedSpacing);
    ui->epsilonSpinBox->setVisible(!isFixedSpacing);
}

void LineResample_Widget::updateDescription() {
    bool isFixedSpacing = (ui->algorithmComboBox->currentIndex() == 0);

    if (isFixedSpacing) {
        ui->descriptionLabel->setText(
                "Fixed Spacing: Resamples the line by creating new points along the line segments "
                "to achieve the desired approximate spacing. First and last points are preserved.");
    } else {
        ui->descriptionLabel->setText(
                "Douglas-Peucker: Simplifies the line by removing points that are within epsilon "
                "distance of the line segment between two endpoints. Preserves the overall shape "
                "while reducing the number of points.");
    }
}

std::unique_ptr<TransformParametersBase> LineResample_Widget::getParameters() const {
    auto params = std::make_unique<LineResampleParameters>();

    // Set the algorithm based on combo box selection
    params->algorithm = (ui->algorithmComboBox->currentIndex() == 0)
                                ? LineSimplificationAlgorithm::FixedSpacing
                                : LineSimplificationAlgorithm::DouglasPeucker;

    // Set the appropriate parameters
    params->target_spacing = static_cast<float>(ui->targetSpacingSpinBox->value());
    params->epsilon = static_cast<float>(ui->epsilonSpinBox->value());

    return params;
}
