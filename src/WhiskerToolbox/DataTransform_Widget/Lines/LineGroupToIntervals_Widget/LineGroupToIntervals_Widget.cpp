#include "LineGroupToIntervals_Widget.hpp"
#include "ui_LineGroupToIntervals_Widget.h"

#include "DataManager/transforms/Lines/Line_Group_To_Intervals/line_group_to_intervals.hpp"
#include "DataManager/DataManager.hpp"
#include "Entity/EntityGroupManager.hpp"

#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>

LineGroupToIntervals_Widget::LineGroupToIntervals_Widget(QWidget* parent)
    : DataManagerParameter_Widget(parent)
    , ui(new Ui::LineGroupToIntervals_Widget)
{
    ui->setupUi(this);
    setupUI();
    connectSignals();
}

LineGroupToIntervals_Widget::~LineGroupToIntervals_Widget()
{
    delete ui;
}

void LineGroupToIntervals_Widget::setupUI()
{
    // Set reasonable default values
    ui->trackPresenceRadioButton->setChecked(true);
    ui->trackAbsenceRadioButton->setChecked(false);
    ui->minIntervalLengthSpinBox->setValue(1);
    ui->mergeGapThresholdSpinBox->setValue(1);
    
    // Initially disable everything until we have a data manager
    ui->groupComboBox->setEnabled(false);
    ui->trackPresenceRadioButton->setEnabled(false);
    ui->trackAbsenceRadioButton->setEnabled(false);
    ui->minIntervalLengthSpinBox->setEnabled(false);
    ui->mergeGapThresholdSpinBox->setEnabled(false);
    
    updateInfoText();
}

void LineGroupToIntervals_Widget::connectSignals()
{
    // Connect parameter change signals
    connect(ui->groupComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LineGroupToIntervals_Widget::onGroupSelected);
    
    connect(ui->trackPresenceRadioButton, &QRadioButton::toggled,
            this, &LineGroupToIntervals_Widget::onTrackPresenceToggled);
    
    connect(ui->trackAbsenceRadioButton, &QRadioButton::toggled,
            this, &LineGroupToIntervals_Widget::onParametersChanged);
    
    connect(ui->minIntervalLengthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &LineGroupToIntervals_Widget::onParametersChanged);
    
    connect(ui->mergeGapThresholdSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &LineGroupToIntervals_Widget::onParametersChanged);
    
    connect(ui->refreshGroupsButton, &QPushButton::clicked,
            this, &LineGroupToIntervals_Widget::refreshGroupList);
}

std::unique_ptr<TransformParametersBase> LineGroupToIntervals_Widget::getParameters() const
{
    if (!dataManager()) {
        return nullptr;
    }
    
    auto group_manager = dataManager()->getEntityGroupManager();
    if (!group_manager) {
        return nullptr;
    }
    
    auto params = std::make_unique<LineGroupToIntervalsParameters>();
    
    // Set group manager
    params->group_manager = group_manager;
    
    // Get selected group ID from combo box
    if (ui->groupComboBox->currentIndex() >= 0) {
        GroupId group_id = ui->groupComboBox->currentData().toULongLong();
        params->target_group_id = group_id;
    } else {
        // No group selected
        return nullptr;
    }
    
    // Set tracking mode
    params->track_presence = ui->trackPresenceRadioButton->isChecked();
    
    // Set filtering parameters
    params->min_interval_length = ui->minIntervalLengthSpinBox->value();
    params->merge_gap_threshold = ui->mergeGapThresholdSpinBox->value();
    
    return params;
}

void LineGroupToIntervals_Widget::onDataManagerChanged()
{
    // Called when the DataManager changes
    if (dataManager()) {
        auto group_manager = dataManager()->getEntityGroupManager();
        if (!group_manager) {
            // Disable the widget if no group manager
            setEnabled(false);
            ui->statusLabel->setText("⚠ Error: No EntityGroupManager available");
            ui->statusLabel->setStyleSheet("QLabel { color: #cc0000; font-weight: bold; }");
        } else {
            setEnabled(true);
            ui->statusLabel->setText("");
            populateGroupComboBox();
            
            // Enable controls
            ui->groupComboBox->setEnabled(true);
            ui->trackPresenceRadioButton->setEnabled(true);
            ui->trackAbsenceRadioButton->setEnabled(true);
            ui->minIntervalLengthSpinBox->setEnabled(true);
            ui->mergeGapThresholdSpinBox->setEnabled(true);
        }
    } else {
        setEnabled(false);
        ui->statusLabel->setText("⚠ Error: No DataManager available");
        ui->statusLabel->setStyleSheet("QLabel { color: #cc0000; font-weight: bold; }");
    }
}

void LineGroupToIntervals_Widget::onDataManagerDataChanged()
{
    // Refresh the group list when data changes
    populateGroupComboBox();
}

void LineGroupToIntervals_Widget::onParametersChanged()
{
    // Parameters changed - update UI feedback
    updateParametersFromUI();
    updateInfoText();
}

void LineGroupToIntervals_Widget::onTrackPresenceToggled(bool trackPresence)
{
    // Update the description based on tracking mode
    if (trackPresence) {
        ui->modeDescriptionLabel->setText("Create intervals for frames where the group IS detected");
    } else {
        ui->modeDescriptionLabel->setText("Create intervals for frames where the group is NOT detected (gaps)");
    }
    
    onParametersChanged();
}

void LineGroupToIntervals_Widget::onGroupSelected(int index)
{
    if (index >= 0 && dataManager()) {
        auto group_manager = dataManager()->getEntityGroupManager();
        if (group_manager) {
            GroupId group_id = ui->groupComboBox->currentData().toULongLong();
            auto descriptor = group_manager->getGroupDescriptor(group_id);
            
            if (descriptor) {
                QString info = QString("Group contains %1 entities").arg(descriptor->entity_count);
                ui->groupInfoLabel->setText(info);
            } else {
                ui->groupInfoLabel->setText("");
            }
        }
    } else {
        ui->groupInfoLabel->setText("");
    }
    
    onParametersChanged();
}

void LineGroupToIntervals_Widget::refreshGroupList()
{
    populateGroupComboBox();
}

void LineGroupToIntervals_Widget::updateParametersFromUI()
{
    // Validate that a group is selected
    if (ui->groupComboBox->currentIndex() < 0) {
        ui->statusLabel->setText("⚠ Please select a group");
        ui->statusLabel->setStyleSheet("QLabel { color: #cc0000; }");
    } else {
        ui->statusLabel->setText("✓ Ready");
        ui->statusLabel->setStyleSheet("QLabel { color: #008000; font-weight: bold; }");
    }
}

void LineGroupToIntervals_Widget::populateGroupComboBox()
{
    ui->groupComboBox->clear();
    ui->groupInfoLabel->setText("");
    
    if (!dataManager()) {
        return;
    }
    
    auto group_manager = dataManager()->getEntityGroupManager();
    if (!group_manager) {
        return;
    }
    
    auto all_groups = group_manager->getAllGroupDescriptors();
    
    if (all_groups.empty()) {
        ui->groupComboBox->addItem("(No groups available)");
        ui->groupComboBox->setEnabled(false);
        ui->statusLabel->setText("⚠ No groups found. Create groups first using a grouping transform.");
        ui->statusLabel->setStyleSheet("QLabel { color: #cc6600; }");
        return;
    }
    
    ui->groupComboBox->setEnabled(true);
    
    // Sort groups by name for easier selection
    std::ranges::sort(all_groups, [](auto const & a, auto const & b) { return a.name < b.name; });
    
    for (auto const & descriptor : all_groups) {
        QString display_text = QString("%1 (%2 entities)")
            .arg(QString::fromStdString(descriptor.name))
            .arg(descriptor.entity_count);
        
        ui->groupComboBox->addItem(display_text, QVariant::fromValue(descriptor.id));
    }
    
    // Select first group by default
    if (!all_groups.empty()) {
        ui->groupComboBox->setCurrentIndex(0);
        onGroupSelected(0);
    }
}

void LineGroupToIntervals_Widget::updateInfoText()
{
    QString info;
    
    bool track_presence = ui->trackPresenceRadioButton->isChecked();
    int min_length = ui->minIntervalLengthSpinBox->value();
    int merge_gap = ui->mergeGapThresholdSpinBox->value();
    
    if (track_presence) {
        info = "<b>Tracking Presence:</b> Output intervals represent continuous frames where the group is detected.";
    } else {
        info = "<b>Tracking Absence:</b> Output intervals represent continuous frames where the group is NOT detected (gaps).";
    }
    
    info += "<br><br>";
    
    if (min_length > 1) {
        info += QString("<b>Filtering:</b> Intervals shorter than %1 frame(s) will be removed.<br>").arg(min_length);
    }
    
    if (merge_gap > 1) {
        info += QString("<b>Merging:</b> Intervals separated by %1 frame(s) or less will be merged together.<br>").arg(merge_gap);
    }
    
    ui->processingInfoLabel->setText(info);
}

