#include "ClassBalancingWidget.hpp"
#include "ui_ClassBalancingWidget.h" // Include the generated UI header

#include <iostream>

ClassBalancingWidget::ClassBalancingWidget(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::ClassBalancingWidget) // Create an instance of the UI class
{
    ui->setupUi(this); // Set up the UI for this widget
    
    // Connect signals from UI elements
    connect(ui->enableBalancingCheckBox, &QCheckBox::toggled, this, &ClassBalancingWidget::onBalancingToggled);
    connect(ui->ratioSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ClassBalancingWidget::onRatioChanged);
    
    // Initial state update for UI elements controlled by onBalancingToggled
    onBalancingToggled(ui->enableBalancingCheckBox->isChecked());
}

ClassBalancingWidget::~ClassBalancingWidget()
{
    delete ui; // Clean up the UI instance
}

bool ClassBalancingWidget::isBalancingEnabled() const {
    std::cout << "Made it to is balancing enabled" << std::endl;
    return ui->enableBalancingCheckBox->isChecked();
}

double ClassBalancingWidget::getBalancingRatio() const {
    return ui->ratioSpinBox->value();
}

void ClassBalancingWidget::setBalancingEnabled(bool enabled) {
    ui->enableBalancingCheckBox->setChecked(enabled);
}

void ClassBalancingWidget::setBalancingRatio(double ratio) {
    ui->ratioSpinBox->setValue(ratio);
}

void ClassBalancingWidget::updateClassDistribution(const QString& distributionText) {
    ui->classDistributionLabel->setText(distributionText);
    ui->classDistributionLabel->setStyleSheet("QLabel { color: black; font-style: normal; }");
}

void ClassBalancingWidget::clearClassDistribution() {
    ui->classDistributionLabel->setText("Class distribution will be shown here after data selection");
    ui->classDistributionLabel->setStyleSheet("QLabel { color: gray; font-style: italic; }");
}

void ClassBalancingWidget::onBalancingToggled(bool enabled) {
    ui->ratioLabel->setEnabled(enabled);
    ui->ratioSpinBox->setEnabled(enabled);
    // No need to enable/disable groupBox or mainLayout as they are containers
    emit balancingSettingsChanged();
}

void ClassBalancingWidget::onRatioChanged(double ratio) {
    Q_UNUSED(ratio)
    emit balancingSettingsChanged();
} 
