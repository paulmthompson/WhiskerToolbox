#include "LineIndexGrouping_Widget.hpp"
#include "ui_LineIndexGrouping_Widget.h"

#include "DataManager/transforms/Lines/Line_Index_Grouping/line_index_grouping.hpp"
#include "DataManager/DataManager.hpp"

#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>

LineIndexGrouping_Widget::LineIndexGrouping_Widget(QWidget* parent)
    : DataManagerParameter_Widget(parent)
    , ui(new Ui::LineIndexGrouping_Widget)
{
    ui->setupUi(this);
    setupUI();
    connectSignals();
}

LineIndexGrouping_Widget::~LineIndexGrouping_Widget()
{
    delete ui;
}

void LineIndexGrouping_Widget::setupUI()
{
    // Set reasonable default values
    ui->groupNamePrefixLineEdit->setText("Line");
    ui->groupDescriptionTemplateLineEdit->setText("Group {} - lines at vector index {}");
    ui->clearExistingGroupsCheckBox->setChecked(false);
}

void LineIndexGrouping_Widget::connectSignals()
{
    // Connect parameter change signals
    connect(ui->groupNamePrefixLineEdit, &QLineEdit::textChanged,
            this, &LineIndexGrouping_Widget::onParametersChanged);
    
    connect(ui->groupDescriptionTemplateLineEdit, &QLineEdit::textChanged,
            this, &LineIndexGrouping_Widget::onParametersChanged);
    
    connect(ui->clearExistingGroupsCheckBox, &QCheckBox::toggled,
            this, &LineIndexGrouping_Widget::onClearExistingGroupsToggled);
}

std::unique_ptr<TransformParametersBase> LineIndexGrouping_Widget::getParameters() const
{
    if (!dataManager()) {
        return nullptr;
    }
    
    auto group_manager = dataManager()->getEntityGroupManager();
    if (!group_manager) {
        return nullptr;
    }
    
    auto params = std::make_unique<LineIndexGroupingParameters>(group_manager);
    
    // Set parameters from UI
    params->group_name_prefix = ui->groupNamePrefixLineEdit->text().toStdString();
    params->group_description_template = ui->groupDescriptionTemplateLineEdit->text().toStdString();
    params->clear_existing_groups = ui->clearExistingGroupsCheckBox->isChecked();
    
    return params;
}

void LineIndexGrouping_Widget::onDataManagerChanged()
{
    // Called when the DataManager changes
    // Validate that we have a group manager available
    if (dataManager()) {
        auto group_manager = dataManager()->getEntityGroupManager();
        if (!group_manager) {
            // Disable the widget if no group manager
            setEnabled(false);
        } else {
            setEnabled(true);
        }
    } else {
        setEnabled(false);
    }
}

void LineIndexGrouping_Widget::onParametersChanged()
{
    // Parameters changed - parent widget will retrieve them when needed
    updateParametersFromUI();
}

void LineIndexGrouping_Widget::onClearExistingGroupsToggled(bool enabled)
{
    // Update UI or show warning if clearing existing groups
    if (enabled) {
        ui->warningLabel->setText("⚠ Warning: This will delete all existing groups before creating new ones.");
        ui->warningLabel->setVisible(true);
    } else {
        ui->warningLabel->setVisible(false);
    }
    
    onParametersChanged();
}

void LineIndexGrouping_Widget::updateParametersFromUI()
{
    // Validate parameters if needed
    // Currently all parameters are valid by default
}
