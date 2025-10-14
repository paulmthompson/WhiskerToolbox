#include "InvertIntervals_Widget.hpp"

#include "ui_InvertIntervals_Widget.h"

#include "DataManager/transforms/DigitalIntervalSeries/Digital_Interval_Invert/digital_interval_invert.hpp"

#include <QPushButton>
#include <iostream>

InvertIntervals_Widget::InvertIntervals_Widget(QWidget * parent)
    : TransformParameter_Widget(parent),
      ui(new Ui::InvertIntervals_Widget) {
    ui->setupUi(this);

    // Connect the domain type combo box to enable/disable bound fields
    connect(ui->domain_type_combobox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &InvertIntervals_Widget::onDomainTypeChanged);

    // Connect spinbox value changes to validation
    connect(ui->bound_start_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &InvertIntervals_Widget::validateBounds);
    connect(ui->bound_end_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &InvertIntervals_Widget::validateBounds);

    // Initially set up the UI state
    onDomainTypeChanged();
}

InvertIntervals_Widget::~InvertIntervals_Widget() {
    delete ui;
}

std::unique_ptr<TransformParametersBase> InvertIntervals_Widget::getParameters() const {
    auto params = std::make_unique<InvertParams>();

    // Get domain type from combo box
    if (ui->domain_type_combobox->currentIndex() == 0) {
        params->domainType = DomainType::Unbounded;
    } else {
        params->domainType = DomainType::Bounded;
    }

    // Get bound values
    params->boundStart = ui->bound_start_spinbox->value();
    params->boundEnd = ui->bound_end_spinbox->value();

    return params;
}

void InvertIntervals_Widget::onDomainTypeChanged() {
    bool isBounded = ui->domain_type_combobox->currentIndex() == 1; // 1 = Bounded

    // Enable/disable bound controls based on domain type
    ui->bound_start_label->setEnabled(isBounded);
    ui->bound_start_spinbox->setEnabled(isBounded);
    ui->bound_end_label->setEnabled(isBounded);
    ui->bound_end_spinbox->setEnabled(isBounded);

    // Validate bounds when domain type changes
    validateBounds();
}

void InvertIntervals_Widget::validateBounds() {
    // Find the parent widget and the transform button
    auto parent_widget = this->parentWidget();
    while (parent_widget && !parent_widget->findChild<QPushButton *>("do_transform_button")) {
        parent_widget = parent_widget->parentWidget();
    }

    if (parent_widget) {
        auto do_btn = parent_widget->findChild<QPushButton *>("do_transform_button");
        if (do_btn) {
            bool isBounded = ui->domain_type_combobox->currentIndex() == 1; // 1 = Bounded

            if (isBounded) {
                double boundStart = ui->bound_start_spinbox->value();
                double boundEnd = ui->bound_end_spinbox->value();

                // Enable button only if bound start is less than bound end
                do_btn->setEnabled(boundStart < boundEnd);
            } else {
                // For unbounded mode, always enable the button
                do_btn->setEnabled(true);
            }
        }
    }
}
