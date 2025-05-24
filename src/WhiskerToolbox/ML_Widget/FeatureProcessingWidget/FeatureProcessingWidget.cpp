#include "FeatureProcessingWidget.hpp"
#include "ui_FeatureProcessingWidget.h"
#include "DataManager/DataManager.hpp" // Already included in .hpp, but good for explicitness

#include <QTableWidgetItem>
#include <QStringList>
#include <iostream>
#include <algorithm> // For std::remove_if

FeatureProcessingWidget::FeatureProcessingWidget(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::FeatureProcessingWidget)
{
    ui->setupUi(this);

    // Setup baseFeatureTableWidget
    ui->baseFeatureTableWidget->setColumnCount(2);
    QStringList base_headers = {"Feature Name", "Type"};
    ui->baseFeatureTableWidget->setHorizontalHeaderLabels(base_headers);
    ui->baseFeatureTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->baseFeatureTableWidget->verticalHeader()->setVisible(false);
    ui->baseFeatureTableWidget->horizontalHeader()->setStretchLastSection(true);

    // Setup activeFeaturesTableWidget
    ui->activeFeaturesTableWidget->setColumnCount(3);
    QStringList active_headers = {"Output Name", "Base Feature", "Transformation"};
    ui->activeFeaturesTableWidget->setHorizontalHeaderLabels(active_headers);
    ui->activeFeaturesTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->activeFeaturesTableWidget->setSelectionMode(QAbstractItemView::NoSelection);
    ui->activeFeaturesTableWidget->verticalHeader()->setVisible(false);
    ui->activeFeaturesTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->activeFeaturesTableWidget->horizontalHeader()->setStretchLastSection(true);

    connect(ui->baseFeatureTableWidget, &QTableWidget::currentItemChanged, this, &FeatureProcessingWidget::_onBaseFeatureSelectionChanged);
    connect(ui->identityTransformCheckBox, &QCheckBox::toggled, this, &FeatureProcessingWidget::_onIdentityCheckBoxToggled);

    _clearTransformationUI(); // Initial state for transformation UI
    ui->transformationsGroupBox->setEnabled(false); // Disabled until a feature is selected
    _updateActiveFeaturesDisplay(); // Initial call
}

FeatureProcessingWidget::~FeatureProcessingWidget() {
    delete ui;
}

void FeatureProcessingWidget::setDataManager(DataManager* dm) {
    _data_manager = dm;
    // If DataManager has an observer system, connect it here to repopulate if needed
    // For now, populating happens on demand or when set.
    // Example: _data_manager->addObserver([this](){ populateBaseFeatures(); }); 
    // However, this might lead to frequent updates. Consider if this is needed.
}

void FeatureProcessingWidget::populateBaseFeatures() {
    if (!_data_manager) {
        std::cerr << "FeatureProcessingWidget: DataManager not set." << std::endl;
        return;
    }

    ui->baseFeatureTableWidget->setRowCount(0); // Clear existing rows
    ui->baseFeatureTableWidget->blockSignals(true);

    std::vector<std::string> all_keys = _data_manager->getAllKeys();
    for (const auto& key_str : all_keys) {
        DM_DataType type = _data_manager->getType(key_str);
        
        bool is_compatible = false;
        for (const auto& compatible_type : _compatible_data_types) {
            if (type == compatible_type) {
                is_compatible = true;
                break;
            }
        }

        if (is_compatible) {
            int row = ui->baseFeatureTableWidget->rowCount();
            ui->baseFeatureTableWidget->insertRow(row);
            QTableWidgetItem* nameItem = new QTableWidgetItem(QString::fromStdString(key_str));
            QTableWidgetItem* typeItem = new QTableWidgetItem(QString::fromStdString(convert_data_type_to_string(type)));
            
            nameItem->setData(Qt::UserRole, QVariant::fromValue(QString::fromStdString(key_str))); // Store key for later retrieval

            ui->baseFeatureTableWidget->setItem(row, 0, nameItem);
            ui->baseFeatureTableWidget->setItem(row, 1, typeItem);
        }
    }
    ui->baseFeatureTableWidget->blockSignals(false);
    _clearTransformationUI();
    ui->transformationsGroupBox->setEnabled(false);
    _currently_selected_base_feature_key.clear();
    _updateActiveFeaturesDisplay(); // Refresh active features display
}

void FeatureProcessingWidget::_onBaseFeatureSelectionChanged(QTableWidgetItem* current, QTableWidgetItem* previous) {
    Q_UNUSED(previous);
    if (current && current->column() == 0) { // Ensure selection is on the name column / row selection
        _currently_selected_base_feature_key = current->data(Qt::UserRole).toString();
        if (!_currently_selected_base_feature_key.isEmpty()) {
            ui->selectedFeatureNameLabel->setText(QString("Selected: %1").arg(_currently_selected_base_feature_key));
            ui->transformationsGroupBox->setEnabled(true);
            _updateUIForSelectedFeature();
        } else {
            _clearTransformationUI();
            ui->transformationsGroupBox->setEnabled(false);
        }
    } else {
        _clearTransformationUI();
        ui->transformationsGroupBox->setEnabled(false);
        // Do not clear _currently_selected_base_feature_key if a non-0 column was clicked or selection cleared by other means,
        // unless current is nullptr indicating full deselection.
        if (!current) {
             _currently_selected_base_feature_key.clear();
        }
    }
}

void FeatureProcessingWidget::_updateUIForSelectedFeature() {
    if (_currently_selected_base_feature_key.isEmpty() || !_data_manager) {
        _clearTransformationUI(false); // Don't clear selected label if it was just set
        return;
    }

    std::string key_std = _currently_selected_base_feature_key.toStdString();
    bool identity_active = false;

    auto it = _feature_configs.find(key_std);
    if (it != _feature_configs.end()) {
        const auto& transforms = it->second;
        for (const auto& transform : transforms) {
            if (transform.type == TransformationType::Identity) {
                identity_active = true;
                break; 
            }
        }
    }

    ui->identityTransformCheckBox->blockSignals(true);
    ui->identityTransformCheckBox->setChecked(identity_active);
    ui->identityTransformCheckBox->blockSignals(false);
    
    // Update other transformation checkboxes here in the future
}

void FeatureProcessingWidget::_clearTransformationUI(bool clearSelectedLabel) {
    ui->identityTransformCheckBox->blockSignals(true);
    ui->identityTransformCheckBox->setChecked(false);
    ui->identityTransformCheckBox->blockSignals(false);
    
    if (clearSelectedLabel) {
        ui->selectedFeatureNameLabel->setText("Selected: None");
    }
    // Clear other transformation controls here
}

void FeatureProcessingWidget::_onIdentityCheckBoxToggled(bool checked) {
    if (_currently_selected_base_feature_key.isEmpty()) return;
    _addOrUpdateTransformation(TransformationType::Identity, checked);
    emit configurationChanged();
    _updateActiveFeaturesDisplay(); // Update display after config change
}

void FeatureProcessingWidget::_addOrUpdateTransformation(TransformationType type, bool active) {
    if (_currently_selected_base_feature_key.isEmpty()) return;
    std::string key_std = _currently_selected_base_feature_key.toStdString();
    AppliedTransformation new_transform = {type};

    auto& transforms_for_key = _feature_configs[key_std]; // Creates entry if not exists

    if (active) {
        bool found = false;
        for(const auto& t : transforms_for_key) {
            if (t.type == type) {
                found = true;
                break;
            }
        }
        if (!found) {
            transforms_for_key.push_back(new_transform);
        }
    } else {
        transforms_for_key.erase(
            std::remove_if(transforms_for_key.begin(), transforms_for_key.end(),
                           [&](const AppliedTransformation& t) { return t.type == type; }),
            transforms_for_key.end());
        
        // If no transformations remain for this key, remove the key from the map
        if (transforms_for_key.empty()) {
            _feature_configs.erase(key_std);
        }
    }
}

std::vector<FeatureProcessingWidget::ProcessedFeatureInfo> FeatureProcessingWidget::getActiveProcessedFeatures() const {
    std::vector<ProcessedFeatureInfo> active_features;
    for (const auto& pair : _feature_configs) {
        const std::string& base_key = pair.first;
        const auto& transformations = pair.second;
        for (const auto& transform : transformations) {
            ProcessedFeatureInfo info;
            info.base_feature_key = base_key;
            info.transformation = transform;
            // Determine output_feature_name based on transformation type
            switch (transform.type) {
                case TransformationType::Identity:
                    info.output_feature_name = base_key; // Same as original for Identity
                    break;
                // Future cases: 
                // case TransformationType::Absolute:
                //     info.output_feature_name = base_key + "_abs";
                //     break;
                default:
                    info.output_feature_name = base_key + "_processed"; // Fallback
                    break;
            }
            active_features.push_back(info);
        }
    }
    return active_features;
}

// Note: _removeTransformation is not strictly needed with how _addOrUpdateTransformation is structured
// but kept here if a more direct removal logic is preferred for specific cases later.
void FeatureProcessingWidget::_removeTransformation(TransformationType type) {
    if (_currently_selected_base_feature_key.isEmpty()) return;
    std::string key_std = _currently_selected_base_feature_key.toStdString();

    auto it = _feature_configs.find(key_std);
    if (it != _feature_configs.end()) {
        auto& transforms = it->second;
        transforms.erase(
            std::remove_if(transforms.begin(), transforms.end(),
                           [&](const AppliedTransformation& t) { return t.type == type; }),
            transforms.end());
        if (transforms.empty()) {
            _feature_configs.erase(it);
        }
    }
}

void FeatureProcessingWidget::_updateActiveFeaturesDisplay() {
    ui->activeFeaturesTableWidget->setRowCount(0); // Clear existing rows
    ui->activeFeaturesTableWidget->blockSignals(true);

    std::vector<ProcessedFeatureInfo> active_processed_features = getActiveProcessedFeatures();

    for (const auto& info : active_processed_features) {
        int row = ui->activeFeaturesTableWidget->rowCount();
        ui->activeFeaturesTableWidget->insertRow(row);

        QTableWidgetItem* outputNameItem = new QTableWidgetItem(QString::fromStdString(info.output_feature_name));
        QTableWidgetItem* baseNameItem = new QTableWidgetItem(QString::fromStdString(info.base_feature_key));
        
        QString transform_str;
        switch (info.transformation.type) {
            case TransformationType::Identity:
                transform_str = "Identity";
                break;
            // Add cases for other transformations here
            default:
                transform_str = "Unknown";
                break;
        }
        QTableWidgetItem* transformItem = new QTableWidgetItem(transform_str);

        ui->activeFeaturesTableWidget->setItem(row, 0, outputNameItem);
        ui->activeFeaturesTableWidget->setItem(row, 1, baseNameItem);
        ui->activeFeaturesTableWidget->setItem(row, 2, transformItem);
    }

    ui->activeFeaturesTableWidget->resizeColumnsToContents();
    ui->activeFeaturesTableWidget->blockSignals(false);
} 