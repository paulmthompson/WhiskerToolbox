#include "LineResample_Widget.hpp"
#include "ui_LineResample_Widget.h"

#include "DataManager/transforms/Lines/Line_Resample/line_resample.hpp"

LineResample_Widget::LineResample_Widget(QWidget * parent)
    : TransformParameter_Widget(parent),
      ui(new Ui::LineResample_Widget) {
    ui->setupUi(this);

    ui->algorithmComboBox->setItemData(
            0,
            static_cast<int>(LineSimplificationAlgorithm::FixedSpacing));
    ui->algorithmComboBox->setItemData(
            1,
            static_cast<int>(LineSimplificationAlgorithm::DouglasPeucker));
    ui->algorithmComboBox->setItemData(
            2,
            static_cast<int>(LineSimplificationAlgorithm::PolynomialSmooth));

    connect(ui->algorithmComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LineResample_Widget::onAlgorithmChanged);

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
    int const index = ui->algorithmComboBox->currentIndex();
    if (index < 0) {
        return;
    }

    auto const algorithm = static_cast<LineSimplificationAlgorithm>(
            ui->algorithmComboBox->itemData(index).toInt());

    bool const show_spacing = (algorithm == LineSimplificationAlgorithm::FixedSpacing) ||
                              (algorithm == LineSimplificationAlgorithm::PolynomialSmooth);
    bool const show_epsilon = (algorithm == LineSimplificationAlgorithm::DouglasPeucker);
    bool const show_order = (algorithm == LineSimplificationAlgorithm::PolynomialSmooth);

    ui->spacingLabel->setVisible(show_spacing);
    ui->targetSpacingSpinBox->setVisible(show_spacing);
    ui->epsilonLabel->setVisible(show_epsilon);
    ui->epsilonSpinBox->setVisible(show_epsilon);
    ui->polynomialOrderLabel->setVisible(show_order);
    ui->polynomialOrderSpinBox->setVisible(show_order);
}

void LineResample_Widget::updateDescription() {
    int const index = ui->algorithmComboBox->currentIndex();
    if (index < 0) {
        return;
    }

    auto const algorithm = static_cast<LineSimplificationAlgorithm>(
            ui->algorithmComboBox->itemData(index).toInt());

    switch (algorithm) {
        case LineSimplificationAlgorithm::FixedSpacing:
            ui->descriptionLabel->setText(
                    "Fixed Spacing: Resamples the line by creating new points along the line segments "
                    "to achieve the desired approximate spacing. First and last points are preserved.");
            break;
        case LineSimplificationAlgorithm::DouglasPeucker:
            ui->descriptionLabel->setText(
                    "Douglas-Peucker: Simplifies the line by removing points that are within epsilon "
                    "distance of the line segment between two endpoints. Preserves the overall shape "
                    "while reducing the number of points.");
            break;
        case LineSimplificationAlgorithm::PolynomialSmooth:
            ui->descriptionLabel->setText(
                    "Polynomial Smooth: Fits parametric polynomials to the line using arc-length "
                    "parameterization, then resamples the smoothed curve at the target spacing. "
                    "Useful for reducing noise while preserving overall curvature.");
            break;
    }
}

std::unique_ptr<TransformParametersBase> LineResample_Widget::getParameters() const {
    auto params = std::make_unique<LineResampleParameters>();

    int const index = ui->algorithmComboBox->currentIndex();
    if (index >= 0) {
        params->algorithm = static_cast<LineSimplificationAlgorithm>(
                ui->algorithmComboBox->itemData(index).toInt());
    }

    params->target_spacing = static_cast<float>(ui->targetSpacingSpinBox->value());
    params->epsilon = static_cast<float>(ui->epsilonSpinBox->value());
    params->polynomial_order = ui->polynomialOrderSpinBox->value();

    return params;
}
