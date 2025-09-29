#include "LineProximityGrouping_Widget.hpp"
#include "ui_LineProximityGrouping_Widget.h"

#include "DataManager/transforms/Lines/Line_Proximity_Grouping/line_proximity_grouping.hpp"
#include "DataManager/DataManager.hpp"

#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QLabel>

LineProximityGrouping_Widget::LineProximityGrouping_Widget(QWidget* parent)
    : DataManagerParameter_Widget(parent)
    , ui(new Ui::LineProximityGrouping_Widget)
{
    ui->setupUi(this);
    setupUI();
    connectSignals();
}

LineProximityGrouping_Widget::~LineProximityGrouping_Widget()
{
    delete ui;
}

void LineProximityGrouping_Widget::setupUI()
{
    // Set reasonable default values
    ui->distanceThresholdSpinBox->setValue(10.0);
    ui->positionSpinBox->setValue(0.5);
    ui->createNewGroupCheckBox->setChecked(true);
    ui->newGroupNameLineEdit->setText("Ungrouped Lines");
    
    // Update the new group controls based on checkbox state
    updateNewGroupControls();
}

void LineProximityGrouping_Widget::connectSignals()
{
    // Connect parameter change signals
    connect(ui->distanceThresholdSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LineProximityGrouping_Widget::onParametersChanged);
    
    connect(ui->positionSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LineProximityGrouping_Widget::onParametersChanged);
    
    connect(ui->createNewGroupCheckBox, &QCheckBox::toggled,
            this, &LineProximityGrouping_Widget::onCreateNewGroupToggled);
    
    connect(ui->newGroupNameLineEdit, &QLineEdit::textChanged,
            this, &LineProximityGrouping_Widget::onParametersChanged);
}

std::unique_ptr<TransformParametersBase> LineProximityGrouping_Widget::getParameters() const
{
    if (!dataManager()) {
        return nullptr;
    }
    
    auto group_manager = dataManager()->getEntityGroupManager();
    if (!group_manager) {
        return nullptr;
    }
    
    auto params = std::make_unique<LineProximityGroupingParameters>(group_manager);
    
    // Set parameters from UI
    params->distance_threshold = static_cast<float>(ui->distanceThresholdSpinBox->value());
    params->position_along_line = static_cast<float>(ui->positionSpinBox->value());
    params->create_new_group_for_outliers = ui->createNewGroupCheckBox->isChecked();
    params->new_group_name = ui->newGroupNameLineEdit->text().toStdString();
    
    return std::move(params);
}

void LineProximityGrouping_Widget::onDataManagerChanged()
{
    // Called when the DataManager changes
    // We might want to validate that we have a group manager available
    if (dataManager()) {
        auto group_manager = dataManager()->getEntityGroupManager();
        if (!group_manager) {
            // Could show a warning or disable the widget
            setEnabled(false);
        } else {
            setEnabled(true);
        }
    } else {
        setEnabled(false);
    }
}

void LineProximityGrouping_Widget::onParametersChanged()
{
    // Validate parameters
    if (validateParameters()) {
        updateParametersFromUI();
        // Note: Transform widgets typically don't emit parameter change signals
        // The parent widget handles getting parameters when needed
    }
}

void LineProximityGrouping_Widget::onCreateNewGroupToggled(bool enabled)
{
    updateNewGroupControls();
    onParametersChanged();
}

void LineProximityGrouping_Widget::updateParametersFromUI()
{
    // This method could be used to store current parameters if needed
    // For now, we get parameters fresh each time via getParameters()
}

void LineProximityGrouping_Widget::updateNewGroupControls()
{
    bool enabled = ui->createNewGroupCheckBox->isChecked();
    ui->newGroupNameLabel->setEnabled(enabled);
    ui->newGroupNameLineEdit->setEnabled(enabled);
}

bool LineProximityGrouping_Widget::validateParameters() const
{
    // Check that distance threshold is positive
    if (ui->distanceThresholdSpinBox->value() <= 0.0) {
        return false;
    }
    
    // Check that position is in valid range [0,1]
    double position = ui->positionSpinBox->value();
    if (position < 0.0 || position > 1.0) {
        return false;
    }
    
    // Check that if new group creation is enabled, we have a name
    if (ui->createNewGroupCheckBox->isChecked()) {
        if (ui->newGroupNameLineEdit->text().trimmed().isEmpty()) {
            return false;
        }
    }
    
    return true;
}