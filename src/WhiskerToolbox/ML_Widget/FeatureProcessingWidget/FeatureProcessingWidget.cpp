#include "FeatureProcessingWidget.hpp"
#include "ui_FeatureProcessingWidget.h"

#include "DataManager/DataManager.hpp"

#include <QStringList>
#include <QTableWidgetItem>

#include <algorithm>
#include <iostream>

FeatureProcessingWidget::FeatureProcessingWidget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::FeatureProcessingWidget) {
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
    connect(ui->squaredTransformCheckBox, &QCheckBox::toggled, this, &FeatureProcessingWidget::_onSquaredCheckBoxToggled);
    connect(ui->lagLeadTransformCheckBox, &QCheckBox::toggled, this, &FeatureProcessingWidget::_onLagLeadCheckBoxToggled);
    connect(ui->minLagSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &FeatureProcessingWidget::_onMinLagChanged);
    connect(ui->maxLeadSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &FeatureProcessingWidget::_onMaxLeadChanged);

    _clearTransformationUI();                      // Initial state for transformation UI
    ui->transformationsGroupBox->setEnabled(false);// Disabled until a feature is selected
    _updateActiveFeaturesDisplay();                // Initial call
}

FeatureProcessingWidget::~FeatureProcessingWidget() {
    delete ui;
}

void FeatureProcessingWidget::setDataManager(DataManager * dm) {
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

    ui->baseFeatureTableWidget->setRowCount(0);// Clear existing rows
    ui->baseFeatureTableWidget->blockSignals(true);

    std::vector<std::string> all_keys = _data_manager->getAllKeys();
    for (auto const & key_str: all_keys) {
        DM_DataType type = _data_manager->getType(key_str);

        bool is_compatible = false;
        for (auto const & compatible_type: _compatible_data_types) {
            if (type == compatible_type) {
                is_compatible = true;
                break;
            }
        }

        if (is_compatible) {
            int row = ui->baseFeatureTableWidget->rowCount();
            ui->baseFeatureTableWidget->insertRow(row);
            QTableWidgetItem * nameItem = new QTableWidgetItem(QString::fromStdString(key_str));
            QTableWidgetItem * typeItem = new QTableWidgetItem(QString::fromStdString(convert_data_type_to_string(type)));

            nameItem->setData(Qt::UserRole, QVariant::fromValue(QString::fromStdString(key_str)));// Store key for later retrieval

            ui->baseFeatureTableWidget->setItem(row, 0, nameItem);
            ui->baseFeatureTableWidget->setItem(row, 1, typeItem);
        }
    }
    ui->baseFeatureTableWidget->blockSignals(false);
    _clearTransformationUI();
    ui->transformationsGroupBox->setEnabled(false);
    _currently_selected_base_feature_key.clear();
    _updateActiveFeaturesDisplay();// Refresh active features display
}

void FeatureProcessingWidget::_onBaseFeatureSelectionChanged(QTableWidgetItem * current, QTableWidgetItem * previous) {
    Q_UNUSED(previous);
    if (current && current->column() == 0) {// Ensure selection is on the name column / row selection
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
        _clearTransformationUI(false);
        return;
    }

    std::string key_std = _currently_selected_base_feature_key.toStdString();
    bool identity_active = false;
    bool squared_active = false;
    bool lag_lead_active = false;
    WhiskerTransformations::LagLeadParams current_ll_params;// Default constructor will init to 0,0

    auto it = _feature_configs.find(key_std);
    if (it != _feature_configs.end()) {
        auto const & transforms = it->second;// This is a std::vector<AppliedTransformation>
        for (auto const & transform_config: transforms) {
            if (transform_config.type == WhiskerTransformations::TransformationType::Identity) {
                identity_active = true;
                // No params for Identity, or retrieve if they exist:
                // if (auto* params = std::get_if<WhiskerTransformations::IdentityParams>(&transform_config.params)) { ... }
            }
            if (transform_config.type == WhiskerTransformations::TransformationType::Squared) {
                squared_active = true;
            }
            if (transform_config.type == WhiskerTransformations::TransformationType::LagLead) {
                lag_lead_active = true;
                if (auto * params = std::get_if<WhiskerTransformations::LagLeadParams>(&transform_config.params)) {
                    current_ll_params = *params;
                }
            }
        }
    }

    ui->identityTransformCheckBox->blockSignals(true);
    ui->identityTransformCheckBox->setChecked(identity_active);
    ui->identityTransformCheckBox->blockSignals(false);

    ui->squaredTransformCheckBox->blockSignals(true);
    ui->squaredTransformCheckBox->setChecked(squared_active);
    ui->squaredTransformCheckBox->blockSignals(false);

    ui->lagLeadTransformCheckBox->blockSignals(true);
    ui->lagLeadParamsGroupBox->setEnabled(lag_lead_active);
    ui->lagLeadTransformCheckBox->setChecked(lag_lead_active);
    ui->minLagSpinBox->setValue(current_ll_params.min_lag_steps);
    ui->maxLeadSpinBox->setValue(current_ll_params.max_lead_steps);
    ui->lagLeadTransformCheckBox->blockSignals(false);
    // Spinboxes remain connected, their direct change will trigger updates if checkbox is checked.
}

void FeatureProcessingWidget::_clearTransformationUI(bool clearSelectedLabel) {
    ui->identityTransformCheckBox->blockSignals(true);
    ui->identityTransformCheckBox->setChecked(false);
    ui->identityTransformCheckBox->blockSignals(false);

    ui->squaredTransformCheckBox->blockSignals(true);
    ui->squaredTransformCheckBox->setChecked(false);
    ui->squaredTransformCheckBox->blockSignals(false);

    ui->lagLeadTransformCheckBox->blockSignals(true);
    ui->lagLeadTransformCheckBox->setChecked(false);
    ui->minLagSpinBox->setValue(0);
    ui->maxLeadSpinBox->setValue(0);
    ui->lagLeadParamsGroupBox->setEnabled(false);
    ui->lagLeadTransformCheckBox->blockSignals(false);

    if (clearSelectedLabel) {
        ui->selectedFeatureNameLabel->setText("Selected: None");
    }
    // Clear other transformation controls here
    emit configurationChanged();
    _updateActiveFeaturesDisplay();
}

void FeatureProcessingWidget::_onIdentityCheckBoxToggled(bool checked) {
    if (_currently_selected_base_feature_key.isEmpty()) return;
    // Create IdentityParams for the variant
    WhiskerTransformations::ParametersVariant params = WhiskerTransformations::IdentityParams{};
    _addOrUpdateTransformation(WhiskerTransformations::TransformationType::Identity, checked, params);
    emit configurationChanged();
    _updateActiveFeaturesDisplay();
}

void FeatureProcessingWidget::_onSquaredCheckBoxToggled(bool checked) {
    if (_currently_selected_base_feature_key.isEmpty()) return;
    // Create SquaredParams for the variant
    WhiskerTransformations::ParametersVariant params = WhiskerTransformations::SquaredParams{};
    _addOrUpdateTransformation(WhiskerTransformations::TransformationType::Squared, checked, params);
    emit configurationChanged();
    _updateActiveFeaturesDisplay();
}

void FeatureProcessingWidget::_onLagLeadCheckBoxToggled(bool checked) {
    if (_currently_selected_base_feature_key.isEmpty()) return;
    ui->lagLeadParamsGroupBox->setEnabled(checked);
    WhiskerTransformations::LagLeadParams ll_params;
    if (checked) {// Only use spinbox values if checked, otherwise default/cleared params
        ll_params.min_lag_steps = ui->minLagSpinBox->value();
        ll_params.max_lead_steps = ui->maxLeadSpinBox->value();
    }
    _addOrUpdateTransformation(WhiskerTransformations::TransformationType::LagLead, checked, ll_params);
    emit configurationChanged();
    _updateActiveFeaturesDisplay();
}

void FeatureProcessingWidget::_onMinLagChanged(int value) {
    Q_UNUSED(value);
    if (ui->lagLeadTransformCheckBox->isChecked()) {
        _onLagLeadCheckBoxToggled(true);// Re-trigger to update params with new spinbox value
    }
}

void FeatureProcessingWidget::_onMaxLeadChanged(int value) {
    Q_UNUSED(value);
    if (ui->lagLeadTransformCheckBox->isChecked()) {
        _onLagLeadCheckBoxToggled(true);// Re-trigger to update params with new spinbox value
    }
}

void FeatureProcessingWidget::_addOrUpdateTransformation(
        WhiskerTransformations::TransformationType type,
        bool active,
        WhiskerTransformations::ParametersVariant const & params// Updated signature
) {
    if (_currently_selected_base_feature_key.isEmpty()) return;
    std::string key_std = _currently_selected_base_feature_key.toStdString();

    WhiskerTransformations::AppliedTransformation new_transform = {type, params};

    if (type == WhiskerTransformations::TransformationType::LagLead) {
        auto leadlag_params = std::get<WhiskerTransformations::LagLeadParams>(params);
        std::cout << "Lead lag params updated with lead " << leadlag_params.max_lead_steps << " and lag " << leadlag_params.min_lag_steps << std::endl;
    }

    auto & transforms_for_key = _feature_configs[key_std];

    if (active) {
        bool found = false;
        for (auto & t: transforms_for_key) {
            if (t.type == type) {
                found = true;
                t = new_transform; //Update in case params changed (e.g. lead lag).
                break;
            }
        }
        if (!found) {
            transforms_for_key.push_back(new_transform);
        }
    } else {
        transforms_for_key.erase(
                std::remove_if(transforms_for_key.begin(), transforms_for_key.end(),
                               [&](WhiskerTransformations::AppliedTransformation const & t) { return t.type == type; }),
                transforms_for_key.end());

        // If no transformations remain for this key, remove the key from the map
        if (transforms_for_key.empty()) {
            _feature_configs.erase(key_std);
        }
    }
}

std::vector<FeatureProcessingWidget::ProcessedFeatureInfo> FeatureProcessingWidget::getActiveProcessedFeatures() const {
    std::vector<ProcessedFeatureInfo> active_features;
    for (auto const & pair: _feature_configs) {
        std::string const & base_key = pair.first;
        auto const & transformations = pair.second;
        for (auto const & transform: transformations) {
            ProcessedFeatureInfo info;
            info.base_feature_key = base_key;
            info.transformation = transform;
            // Determine output_feature_name based on transformation type
            switch (transform.type) {
                case WhiskerTransformations::TransformationType::Identity:
                    info.output_feature_name = base_key;// Same as original for Identity
                    break;
                case WhiskerTransformations::TransformationType::Squared:
                    info.output_feature_name = base_key + "_sq";
                    break;
                case WhiskerTransformations::TransformationType::LagLead:
                    if (auto * ll_params = std::get_if<WhiskerTransformations::LagLeadParams>(&transform.params)) {
                        info.output_feature_name = base_key + "_ll_m" + std::to_string(ll_params->min_lag_steps) + "_p" + std::to_string(ll_params->max_lead_steps);
                    } else {
                        info.output_feature_name = base_key + "_laglead_invalidparams";
                    }
                    break;
                default:
                    info.output_feature_name = base_key + "_processed";// Fallback
                    break;
            }
            active_features.push_back(info);
        }
    }
    return active_features;
}

// Note: _removeTransformation is not strictly needed with how _addOrUpdateTransformation is structured
// but kept here if a more direct removal logic is preferred for specific cases later.
void FeatureProcessingWidget::_removeTransformation(WhiskerTransformations::TransformationType type) {
    if (_currently_selected_base_feature_key.isEmpty()) return;
    std::string key_std = _currently_selected_base_feature_key.toStdString();

    auto it = _feature_configs.find(key_std);
    if (it != _feature_configs.end()) {
        auto & transforms = it->second;
        transforms.erase(
                std::remove_if(transforms.begin(), transforms.end(),
                               [&](WhiskerTransformations::AppliedTransformation const & t) { return t.type == type; }),
                transforms.end());
        if (transforms.empty()) {
            _feature_configs.erase(it);
        }
    }
}

void FeatureProcessingWidget::_updateActiveFeaturesDisplay() {
    ui->activeFeaturesTableWidget->setRowCount(0);// Clear existing rows
    ui->activeFeaturesTableWidget->blockSignals(true);

    std::vector<ProcessedFeatureInfo> active_processed_features = getActiveProcessedFeatures();

    for (auto const & info: active_processed_features) {
        int row = ui->activeFeaturesTableWidget->rowCount();
        ui->activeFeaturesTableWidget->insertRow(row);

        QTableWidgetItem * outputNameItem = new QTableWidgetItem(QString::fromStdString(info.output_feature_name));
        QTableWidgetItem * baseNameItem = new QTableWidgetItem(QString::fromStdString(info.base_feature_key));

        QString transform_str;
        switch (info.transformation.type) {
            case WhiskerTransformations::TransformationType::Identity:
                transform_str = "Identity";
                break;
            case WhiskerTransformations::TransformationType::Squared:
                transform_str = "Squared";
                break;
            case WhiskerTransformations::TransformationType::LagLead:
                if (auto * ll_params = std::get_if<WhiskerTransformations::LagLeadParams>(&info.transformation.params)) {
                    transform_str = QString("Lag/Lead (Lag: %1, Lead: %2)").arg(ll_params->min_lag_steps).arg(ll_params->max_lead_steps);
                } else {
                    transform_str = "Lag/Lead (Error)";
                }
                break;
            default:
                transform_str = "Unknown";
                break;
        }
        QTableWidgetItem * transformItem = new QTableWidgetItem(transform_str);

        ui->activeFeaturesTableWidget->setItem(row, 0, outputNameItem);
        ui->activeFeaturesTableWidget->setItem(row, 1, baseNameItem);
        ui->activeFeaturesTableWidget->setItem(row, 2, transformItem);
    }

    ui->activeFeaturesTableWidget->resizeColumnsToContents();
    ui->activeFeaturesTableWidget->blockSignals(false);
}
