#include "MaskMedianFilter_Widget.hpp"
#include "ui_MaskMedianFilter_Widget.h"

MaskMedianFilter_Widget::MaskMedianFilter_Widget(QWidget *parent) :
    TransformParameter_Widget(parent),
    ui(new Ui::MaskMedianFilter_Widget)
{
    ui->setupUi(this);
    
    // Set default window size to 3
    ui->windowSizeSpinBox->setValue(3);
    
    // Set minimum and maximum window sizes (must be odd)
    ui->windowSizeSpinBox->setMinimum(1);
    ui->windowSizeSpinBox->setMaximum(15);
    ui->windowSizeSpinBox->setSingleStep(2); // Only allow odd values
    
    // Connect signal to ensure only odd values
    connect(ui->windowSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MaskMedianFilter_Widget::onWindowSizeChanged);
}

MaskMedianFilter_Widget::~MaskMedianFilter_Widget()
{
    delete ui;
}

void MaskMedianFilter_Widget::onWindowSizeChanged(int value)
{
    // Ensure window size is always odd
    if (value % 2 == 0) {
        ui->windowSizeSpinBox->setValue(value + 1);
    }
}

std::unique_ptr<TransformParametersBase> MaskMedianFilter_Widget::getParameters() const
{
    auto params = std::make_unique<MaskMedianFilterParameters>();
    params->window_size = ui->windowSizeSpinBox->value();
    return params;
} 