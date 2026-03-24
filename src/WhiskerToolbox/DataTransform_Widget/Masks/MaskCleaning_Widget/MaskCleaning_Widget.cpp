#include "MaskCleaning_Widget.hpp"
#include "ui_MaskCleaning_Widget.h"

#include "DataManager/transforms/Masks/Mask_Cleaning/mask_cleaning.hpp"

MaskCleaning_Widget::MaskCleaning_Widget(QWidget * parent)
    : TransformParameter_Widget(parent),
      ui(new Ui::MaskCleaning_Widget) {
    ui->setupUi(this);
    ui->selectionComboBox->setCurrentIndex(0);
    ui->countSpinBox->setMinimum(1);
    ui->countSpinBox->setMaximum(100);
    ui->countSpinBox->setValue(1);
}

MaskCleaning_Widget::~MaskCleaning_Widget() {
    delete ui;
}

std::unique_ptr<TransformParametersBase> MaskCleaning_Widget::getParameters() const {
    auto params = std::make_unique<MaskCleaningParameters>();
    QString const sel = ui->selectionComboBox->currentText();
    params->selection = (sel == QStringLiteral("Smallest")) ? MaskCleaningSelection::Smallest
                                                            : MaskCleaningSelection::Largest;
    params->count = ui->countSpinBox->value();
    return params;
}
