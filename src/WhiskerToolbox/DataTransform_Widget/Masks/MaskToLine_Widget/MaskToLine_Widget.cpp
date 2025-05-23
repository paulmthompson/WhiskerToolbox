#include "MaskToLine_Widget.hpp"
#include "ui_MaskToLine_Widget.h"

MaskToLine_Widget::MaskToLine_Widget(QWidget *parent) :
    TransformParameter_Widget(parent),
    ui(new Ui::MaskToLine_Widget),
    _data_manager(nullptr)
{
    ui->setupUi(this);
    
    // Set default values
    ui->polynomialOrderSpinBox->setValue(3);
    ui->errorThresholdSpinBox->setValue(5.0);
    ui->subsampleSpinBox->setValue(1);
    ui->removeOutliersCheckBox->setChecked(true);
    ui->smoothLineCheckBox->setChecked(false);
    ui->outputResolutionSpinBox->setValue(5.0);
    
    // Set up method combo box
    ui->methodComboBox->addItem("Skeletonize", static_cast<int>(LinePointSelectionMethod::Skeletonize));
    ui->methodComboBox->addItem("Nearest to Reference", static_cast<int>(LinePointSelectionMethod::NearestToReference));
    ui->methodComboBox->setCurrentIndex(0);
    
    // Connect signals
    connect(ui->methodComboBox, &QComboBox::currentIndexChanged, this, &MaskToLine_Widget::onMethodChanged);
}

MaskToLine_Widget::~MaskToLine_Widget()
{
    delete ui;
}

void MaskToLine_Widget::setDataManager(std::shared_ptr<DataManager> data_manager)
{
    _data_manager = std::move(data_manager);
}

void MaskToLine_Widget::onMethodChanged(int index)
{
    if (index == 0) { // Skeletonize
        ui->skeletonizeDescriptionLabel->setVisible(true);
    } else { // Nearest to Reference
        ui->skeletonizeDescriptionLabel->setVisible(false);
    }
}

std::unique_ptr<TransformParametersBase> MaskToLine_Widget::getParameters() const
{
    auto params = std::make_unique<MaskToLineParameters>();
    
    // Reference point parameters
    params->reference_x = static_cast<float>(ui->referenceXSpinBox->value());
    params->reference_y = static_cast<float>(ui->referenceYSpinBox->value());
    
    // Method
    int methodIndex = ui->methodComboBox->currentIndex();
    if (methodIndex == 0) {
        params->method = LinePointSelectionMethod::Skeletonize;
    } else {
        params->method = LinePointSelectionMethod::NearestToReference;
    }
    
    // Quality parameters
    params->polynomial_order = ui->polynomialOrderSpinBox->value();
    params->error_threshold = static_cast<float>(ui->errorThresholdSpinBox->value());
    params->remove_outliers = ui->removeOutliersCheckBox->isChecked();
    params->input_point_subsample_factor = ui->subsampleSpinBox->value();
    params->should_smooth_line = ui->smoothLineCheckBox->isChecked();
    params->output_resolution = static_cast<float>(ui->outputResolutionSpinBox->value());
    
    return params;
} 