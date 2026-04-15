/**
 * @file DimReductionPanel.cpp
 * @brief Implementation of the dimensionality reduction panel
 */

#include "DimReductionPanel.hpp"

#include "Core/MLCoreWidgetState.hpp"
#include "ui_DimReductionPanel.h"

#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "MLCore/models/MLModelParameters.hpp"
#include "MLCore/models/MLModelRegistry.hpp"
#include "MLCore/models/MLTaskType.hpp"
#include "Tensors/TensorData.hpp"

#include <QListWidget>
#include <QRadioButton>
#include <QStackedWidget>
#include <QVariant>

#include <algorithm>
#include <cstddef>
#include <numeric>

// =============================================================================
// Construction / destruction
// =============================================================================

DimReductionPanel::DimReductionPanel(
        std::shared_ptr<MLCoreWidgetState> state,
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent)
    : QWidget(parent),
      ui(new Ui::DimReductionPanel),
      _state(std::move(state)),
      _data_manager(std::move(data_manager)),
      _registry(std::make_unique<MLCore::MLModelRegistry>()) {
    ui->setupUi(this);
    _populateAlgorithms();
    _populateLabelSourceCombo();
    _setupConnections();
    _registerDataManagerObserver();

    // Default: show PCA params, hide t-SNE and Robust PCA and LARS params
    ui->tsneParamsWidget->setVisible(false);
    ui->robustPcaParamsWidget->setVisible(false);
    ui->larsParamsWidget->setVisible(false);

    // Default: unsupervised mode — labels hidden
    ui->labelsGroupBox->setVisible(false);

    refreshTensorList();
    _restoreFromState();
}

DimReductionPanel::~DimReductionPanel() {
    if (_dm_observer_id >= 0 && _data_manager) {
        _data_manager->removeObserver(_dm_observer_id);
    }
    if (_group_observer_id >= 0 && _data_manager) {
        auto * group_mgr = _data_manager->getEntityGroupManager();
        if (group_mgr) {
            group_mgr->getGroupObservers().removeObserver(_group_observer_id);
        }
    }
    delete ui;
}

// =============================================================================
// Public API
// =============================================================================

std::string DimReductionPanel::selectedTensorKey() const {
    int const idx = ui->tensorComboBox->currentIndex();
    if (idx < 0) {
        return {};
    }
    QVariant const data = ui->tensorComboBox->itemData(idx);
    if (data.isValid()) {
        return data.toString().toStdString();
    }
    return ui->tensorComboBox->currentText().toStdString();
}

std::string DimReductionPanel::selectedAlgorithmName() const {
    int const idx = ui->algorithmComboBox->currentIndex();
    if (idx < 0) {
        return {};
    }
    return ui->algorithmComboBox->currentData().toString().toStdString();
}

int DimReductionPanel::nComponents() const {
    return ui->nComponentsSpinBox->value();
}

bool DimReductionPanel::zscoreNormalize() const {
    return ui->zscoreCheckBox->isChecked();
}

std::string DimReductionPanel::outputKey() const {
    return ui->outputKeyLineEdit->text().toStdString();
}

bool DimReductionPanel::isSupervisedMode() const {
    return ui->supervisedRadio->isChecked();
}

bool DimReductionPanel::hasValidConfiguration() const {
    if (selectedTensorKey().empty() || outputKey().empty()) {
        return false;
    }

    if (isSupervisedMode()) {
        // Supervised: need a valid algorithm + label config
        if (selectedAlgorithmName().empty()) {
            return false;
        }
        auto const source = labelSourceType();
        if (source == "intervals") {
            return !labelIntervalKey().empty();
        }
        if (source == "groups") {
            return _selected_group_ids.size() >= 2;
        }
        if (source == "entity_groups") {
            return _selected_data_group_ids.size() >= 2 && !labelDataKey().empty();
        }
        if (source == "events") {
            return !labelEventKey().empty();
        }
        return false;
    }

    // Unsupervised: just need an algorithm
    return !selectedAlgorithmName().empty();
}

std::unique_ptr<MLCore::MLModelParametersBase> DimReductionPanel::currentParameters() const {
    auto const name = selectedAlgorithmName();

    if (name == "PCA") {
        auto params = std::make_unique<MLCore::PCAParameters>();
        params->n_components = static_cast<std::size_t>(nComponents());
        return params;
    }

    if (name == "t-SNE") {
        auto params = std::make_unique<MLCore::TSNEParameters>();
        params->n_components = static_cast<std::size_t>(ui->tsneComponentsSpinBox->value());
        params->perplexity = ui->perplexitySpinBox->value();
        params->theta = ui->thetaSpinBox->value();
        return params;
    }

    if (name == "Robust PCA") {
        auto params = std::make_unique<MLCore::RobustPCAParameters>();
        params->n_components = static_cast<std::size_t>(ui->rpcaComponentsSpinBox->value());
        params->lambda = ui->rpcaLambdaSpinBox->value();
        params->max_iter = static_cast<std::size_t>(ui->rpcaMaxIterSpinBox->value());
        return params;
    }

    if (name == "Supervised PCA") {
        auto params = std::make_unique<MLCore::SupervisedPCAParameters>();
        params->n_components = static_cast<std::size_t>(nComponents());
        return params;
    }

    if (name == "Supervised Robust PCA") {
        auto params = std::make_unique<MLCore::SupervisedRobustPCAParameters>();
        params->n_components = static_cast<std::size_t>(ui->rpcaComponentsSpinBox->value());
        params->lambda = ui->rpcaLambdaSpinBox->value();
        params->max_iter = static_cast<std::size_t>(ui->rpcaMaxIterSpinBox->value());
        return params;
    }

    if (name == "LARS Projection") {
        auto params = std::make_unique<MLCore::LARSProjectionParameters>();
        int const reg_idx = ui->larsRegTypeCombo->currentIndex();
        if (reg_idx == 0) {
            params->regularization_type = MLCore::RegularizationType::LASSO;
        } else if (reg_idx == 1) {
            params->regularization_type = MLCore::RegularizationType::Ridge;
        } else {
            params->regularization_type = MLCore::RegularizationType::ElasticNet;
        }
        params->lambda1 = ui->larsLambda1SpinBox->value();
        params->lambda2 = ui->larsLambda2SpinBox->value();
        return params;
    }

    return nullptr;
}

// =============================================================================
// Supervised mode accessors
// =============================================================================

std::string DimReductionPanel::labelSourceType() const {
    int const idx = ui->labelSourceCombo->currentIndex();
    if (idx < 0) {
        return "intervals";
    }
    return ui->labelSourceCombo->currentData().toString().toStdString();
}

std::string DimReductionPanel::labelIntervalKey() const {
    int const idx = ui->intervalKeyCombo->currentIndex();
    if (idx < 0) {
        return {};
    }
    QVariant const data = ui->intervalKeyCombo->itemData(idx);
    if (data.isValid() && !data.toString().isEmpty()) {
        return data.toString().toStdString();
    }
    return {};
}

std::string DimReductionPanel::labelPositiveClassName() const {
    return ui->positiveClassEdit->text().toStdString();
}

std::string DimReductionPanel::labelNegativeClassName() const {
    return ui->negativeClassEdit->text().toStdString();
}

std::string DimReductionPanel::labelEventKey() const {
    int const idx = ui->eventKeyCombo->currentIndex();
    if (idx < 0) {
        return {};
    }
    QVariant const data = ui->eventKeyCombo->itemData(idx);
    if (data.isValid() && !data.toString().isEmpty()) {
        return data.toString().toStdString();
    }
    return {};
}

std::vector<uint64_t> DimReductionPanel::selectedGroupIds() const {
    if (labelSourceType() == "groups") {
        return _selected_group_ids;
    }
    if (labelSourceType() == "entity_groups") {
        return _selected_data_group_ids;
    }
    return {};
}

std::string DimReductionPanel::labelDataKey() const {
    int const idx = ui->dataKeyCombo->currentIndex();
    if (idx < 0) {
        return {};
    }
    QVariant const data = ui->dataKeyCombo->itemData(idx);
    if (data.isValid() && !data.toString().isEmpty()) {
        return data.toString().toStdString();
    }
    return {};
}

void DimReductionPanel::showResults(
        std::size_t num_observations,
        std::size_t num_input_features,
        std::size_t num_output_components,
        std::size_t rows_dropped,
        std::vector<double> const & explained_variance) {

    QString results_text = QStringLiteral(
                                   "<b>Observations:</b> %1<br>"
                                   "<b>Input features:</b> %2<br>"
                                   "<b>Output components:</b> %3")
                                   .arg(num_observations)
                                   .arg(num_input_features)
                                   .arg(num_output_components);

    if (rows_dropped > 0) {
        results_text += QStringLiteral("<br><b>Rows dropped (NaN):</b> %1")
                                .arg(rows_dropped);
    }

    ui->resultsLabel->setStyleSheet(QString{});
    ui->resultsLabel->setText(results_text);

    if (!explained_variance.empty()) {
        QString var_text = QStringLiteral("<b>Explained variance:</b><br>");
        double cumulative = 0.0;
        for (std::size_t i = 0; i < explained_variance.size(); ++i) {
            cumulative += explained_variance[i];
            var_text += QStringLiteral("  PC%1: %2% (cumulative: %3%)<br>")
                                .arg(i + 1)
                                .arg(explained_variance[i] * 100.0, 0, 'f', 1)
                                .arg(cumulative * 100.0, 0, 'f', 1);
        }
        ui->varianceLabel->setText(var_text);
    } else {
        ui->varianceLabel->setText(QString{});
    }
}

void DimReductionPanel::showSupervisedResults(
        std::size_t num_observations,
        std::size_t num_training_observations,
        std::size_t num_input_features,
        std::size_t num_output_dimensions,
        std::size_t rows_dropped,
        std::size_t unlabeled_count,
        std::size_t num_classes,
        std::vector<std::string> const & class_names) {

    QString results_text = QStringLiteral(
                                   "<b>Total observations:</b> %1<br>"
                                   "<b>Training observations:</b> %2<br>"
                                   "<b>Input features:</b> %3<br>"
                                   "<b>Output dimensions:</b> %4<br>"
                                   "<b>Classes:</b> %5")
                                   .arg(num_observations)
                                   .arg(num_training_observations)
                                   .arg(num_input_features)
                                   .arg(num_output_dimensions)
                                   .arg(num_classes);

    if (rows_dropped > 0) {
        results_text += QStringLiteral("<br><b>Rows dropped (NaN):</b> %1")
                                .arg(rows_dropped);
    }

    if (unlabeled_count > 0) {
        results_text += QStringLiteral("<br><b>Unlabeled rows:</b> %1")
                                .arg(unlabeled_count);
    }

    ui->resultsLabel->setStyleSheet(QString{});
    ui->resultsLabel->setText(results_text);

    if (!class_names.empty()) {
        QString class_text = QStringLiteral("<b>Class names:</b><br>");
        for (std::size_t i = 0; i < class_names.size(); ++i) {
            class_text += QStringLiteral("  %1: %2<br>")
                                  .arg(i)
                                  .arg(QString::fromStdString(class_names[i]));
        }
        ui->varianceLabel->setText(class_text);
    } else {
        ui->varianceLabel->setText(QString{});
    }
}

void DimReductionPanel::clearResults() {
    ui->resultsLabel->setStyleSheet(
            QStringLiteral("QLabel { color: gray; font-style: italic; }"));
    ui->resultsLabel->setText(QStringLiteral("No results yet"));
    ui->varianceLabel->setText(QString{});
}

// =============================================================================
// Public slots
// =============================================================================

void DimReductionPanel::refreshTensorList() {
    if (!_data_manager) {
        return;
    }

    QString current_selection;
    int const cur_idx = ui->tensorComboBox->currentIndex();
    if (cur_idx >= 0) {
        QVariant const data = ui->tensorComboBox->itemData(cur_idx);
        if (data.isValid()) {
            current_selection = data.toString();
        } else {
            current_selection = ui->tensorComboBox->currentText();
        }
    }

    ui->tensorComboBox->blockSignals(true);
    ui->tensorComboBox->clear();

    // Empty sentinel
    ui->tensorComboBox->addItem(QString{});

    auto tensor_keys = _data_manager->getKeys<TensorData>();
    std::sort(tensor_keys.begin(), tensor_keys.end());

    for (auto const & key: tensor_keys) {
        auto tensor = _data_manager->getData<TensorData>(key);
        if (tensor) {
            QString const display = QStringLiteral("%1 (%2x%3)")
                                            .arg(QString::fromStdString(key))
                                            .arg(tensor->numRows())
                                            .arg(tensor->numColumns());
            ui->tensorComboBox->addItem(display, QString::fromStdString(key));
        } else {
            ui->tensorComboBox->addItem(QString::fromStdString(key),
                                        QString::fromStdString(key));
        }
    }

    // Restore previous selection
    int restored_idx = -1;
    for (int i = 0; i < ui->tensorComboBox->count(); ++i) {
        if (ui->tensorComboBox->itemData(i).toString() == current_selection) {
            restored_idx = i;
            break;
        }
    }

    if (restored_idx != -1) {
        ui->tensorComboBox->setCurrentIndex(restored_idx);
    } else {
        ui->tensorComboBox->setCurrentIndex(0);
    }

    ui->tensorComboBox->blockSignals(false);

    _updateTensorInfo();
}

// =============================================================================
// Public slots — label sources
// =============================================================================

void DimReductionPanel::refreshLabelSources() {
    _refreshIntervalCombo();
    _refreshGroupCombo();
    _refreshDataKeyCombo();
    _refreshDataGroupCombo();
    _refreshEventCombo();
}

// =============================================================================
// Private slots
// =============================================================================

void DimReductionPanel::_onTensorComboChanged(int index) {
    QString key_str;
    if (index >= 0) {
        QVariant const data = ui->tensorComboBox->itemData(index);
        if (data.isValid()) {
            key_str = data.toString();
        } else {
            key_str = ui->tensorComboBox->itemText(index);
        }
    }

    if (_state) {
        _state->setDimReductionTensorKey(key_str.toStdString());
    }

    _updateTensorInfo();
    _updateOutputKeyFromInput();
    emit tensorSelectionChanged(key_str);
}

void DimReductionPanel::_onAlgorithmChanged(int index) {
    if (_updating) {
        return;
    }

    if (index < 0) {
        if (_state) {
            _state->setDimReductionModelName({});
        }
        return;
    }

    auto const name = ui->algorithmComboBox->currentData().toString().toStdString();
    if (_state) {
        _state->setDimReductionModelName(name);
    }

    // Toggle algorithm-specific parameter widgets
    if (isSupervisedMode()) {
        // Supervised PCA reuses the PCA params widget (n_components + scale)
        bool const is_spca = (name == "Supervised PCA");
        bool const is_srpca = (name == "Supervised Robust PCA");
        bool const is_lars = (name == "LARS Projection");
        ui->pcaParamsWidget->setVisible(is_spca);
        ui->tsneParamsWidget->setVisible(false);
        ui->robustPcaParamsWidget->setVisible(is_srpca);
        ui->larsParamsWidget->setVisible(is_lars);
        if (is_lars) {
            _updateLarsRegularizationUI(ui->larsRegTypeCombo->currentIndex());
        }

        // Hide class name fields for non-discriminative algorithms (e.g. sPCA)
        // where output dimensions are user-controlled, not class-count-based.
        bool const show_class_names = !is_spca && !is_srpca;
        ui->positiveClassLabel->setVisible(show_class_names);
        ui->positiveClassEdit->setVisible(show_class_names);
        ui->negativeClassLabel->setVisible(show_class_names);
        ui->negativeClassEdit->setVisible(show_class_names);
    } else {
        bool const is_pca = (name == "PCA");
        bool const is_tsne = (name == "t-SNE");
        bool const is_rpca = (name == "Robust PCA");
        ui->pcaParamsWidget->setVisible(is_pca);
        ui->tsneParamsWidget->setVisible(is_tsne);
        ui->robustPcaParamsWidget->setVisible(is_rpca);
        ui->larsParamsWidget->setVisible(false);
    }

    _updateOutputKeyFromInput();
}

void DimReductionPanel::_updateLarsRegularizationUI(int regTypeIndex) {
    // 0 = LASSO (L1 only), 1 = Ridge (L2 only), 2 = Elastic Net (both)
    bool const enable_l1 = (regTypeIndex != 1);
    bool const enable_l2 = (regTypeIndex != 0);
    ui->larsLambda1SpinBox->setEnabled(enable_l1);
    ui->larsLambda2SpinBox->setEnabled(enable_l2);
}

void DimReductionPanel::_onModeToggled(bool supervised) {
    _updateSupervisedVisibility(supervised);

    if (_state) {
        _state->setDimReductionSupervised(supervised);
    }

    // Repopulate algorithm list for the new mode
    _populateAlgorithms();

    // Refresh label combos if switching to supervised
    if (supervised) {
        refreshLabelSources();
    }

    _updateOutputKeyFromInput();
}

void DimReductionPanel::_onLabelSourceChanged(int index) {
    if (index < 0) {
        return;
    }

    ui->labelStack->setCurrentIndex(index);

    auto const type = ui->labelSourceCombo->currentData().toString().toStdString();
    if (_state) {
        _state->setDimReductionLabelSourceType(type);
    }
}

void DimReductionPanel::_onAddGroupClicked() {
    int const idx = ui->groupCombo->currentIndex();
    if (idx < 0) {
        return;
    }
    auto const group_id = ui->groupCombo->currentData().toULongLong();

    // Don't add duplicates
    if (std::find(_selected_group_ids.begin(), _selected_group_ids.end(), group_id) !=
        _selected_group_ids.end()) {
        return;
    }

    _selected_group_ids.push_back(group_id);
    _rebuildGroupClassList();
    _syncGroupIdsToState();
}

void DimReductionPanel::_onRemoveGroupClicked() {
    auto const selected_items = ui->groupClassList->selectedItems();
    if (selected_items.isEmpty()) {
        return;
    }

    auto const group_id = selected_items.first()->data(Qt::UserRole).toULongLong();
    _selected_group_ids.erase(
            std::remove(_selected_group_ids.begin(), _selected_group_ids.end(), group_id),
            _selected_group_ids.end());
    _rebuildGroupClassList();
    _syncGroupIdsToState();
}

void DimReductionPanel::_onAddDataGroupClicked() {
    int const idx = ui->dataGroupCombo->currentIndex();
    if (idx < 0) {
        return;
    }
    auto const group_id = ui->dataGroupCombo->currentData().toULongLong();

    if (std::find(_selected_data_group_ids.begin(), _selected_data_group_ids.end(), group_id) !=
        _selected_data_group_ids.end()) {
        return;
    }

    _selected_data_group_ids.push_back(group_id);
    _rebuildDataGroupClassList();
    _syncGroupIdsToState();
}

void DimReductionPanel::_onRemoveDataGroupClicked() {
    auto const selected_items = ui->dataGroupClassList->selectedItems();
    if (selected_items.isEmpty()) {
        return;
    }

    auto const group_id = selected_items.first()->data(Qt::UserRole).toULongLong();
    _selected_data_group_ids.erase(
            std::remove(_selected_data_group_ids.begin(), _selected_data_group_ids.end(), group_id),
            _selected_data_group_ids.end());
    _rebuildDataGroupClassList();
    _syncGroupIdsToState();
}

void DimReductionPanel::_onRunClicked() {
    _syncToState();
    if (isSupervisedMode()) {
        emit supervisedRunRequested();
    } else {
        emit runRequested();
    }
}

// =============================================================================
// Setup helpers
// =============================================================================

void DimReductionPanel::_setupConnections() {
    connect(ui->tensorComboBox, &QComboBox::currentIndexChanged,
            this, &DimReductionPanel::_onTensorComboChanged);

    connect(ui->refreshButton, &QPushButton::clicked,
            this, &DimReductionPanel::refreshTensorList);

    connect(ui->algorithmComboBox, &QComboBox::currentIndexChanged,
            this, &DimReductionPanel::_onAlgorithmChanged);

    connect(ui->runButton, &QPushButton::clicked,
            this, &DimReductionPanel::_onRunClicked);

    connect(ui->zscoreCheckBox, &QCheckBox::toggled,
            this, [this](bool checked) {
                if (!_updating && _state) {
                    _state->setDimReductionZscoreNormalize(checked);
                }
            });

    connect(ui->nComponentsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int value) {
                if (!_updating && _state) {
                    _state->setDimReductionNComponents(value);
                }
            });

    connect(ui->outputKeyLineEdit, &QLineEdit::textChanged,
            this, [this](QString const & text) {
                if (!_updating && _state) {
                    _state->setDimReductionOutputKey(text.toStdString());
                }
            });

    // Mode radio buttons
    connect(ui->supervisedRadio, &QRadioButton::toggled,
            this, &DimReductionPanel::_onModeToggled);

    // LARS regularization type → enable/disable lambda spin boxes
    connect(ui->larsRegTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DimReductionPanel::_updateLarsRegularizationUI);

    // Label source combo
    connect(ui->labelSourceCombo, &QComboBox::currentIndexChanged,
            this, &DimReductionPanel::_onLabelSourceChanged);

    // Interval mode
    connect(ui->intervalKeyCombo, &QComboBox::currentIndexChanged,
            this, [this](int /*index*/) {
                if (!_updating && _state) {
                    _state->setDimReductionLabelIntervalKey(labelIntervalKey());
                }
            });
    connect(ui->positiveClassEdit, &QLineEdit::textChanged,
            this, [this](QString const & text) {
                if (!_updating && _state) {
                    _state->setDimReductionLabelPositiveClass(text.toStdString());
                }
            });
    connect(ui->negativeClassEdit, &QLineEdit::textChanged,
            this, [this](QString const & text) {
                if (!_updating && _state) {
                    _state->setDimReductionLabelNegativeClass(text.toStdString());
                }
            });

    // Group mode
    connect(ui->addGroupButton, &QPushButton::clicked,
            this, &DimReductionPanel::_onAddGroupClicked);
    connect(ui->removeGroupButton, &QPushButton::clicked,
            this, &DimReductionPanel::_onRemoveGroupClicked);

    // Data group mode
    connect(ui->dataKeyCombo, &QComboBox::currentIndexChanged,
            this, [this](int /*index*/) {
                if (!_updating && _state) {
                    _state->setDimReductionLabelDataKey(labelDataKey());
                }
            });
    connect(ui->addDataGroupButton, &QPushButton::clicked,
            this, &DimReductionPanel::_onAddDataGroupClicked);
    connect(ui->removeDataGroupButton, &QPushButton::clicked,
            this, &DimReductionPanel::_onRemoveDataGroupClicked);

    // Event mode
    connect(ui->eventKeyCombo, &QComboBox::currentIndexChanged,
            this, [this](int /*index*/) {
                if (!_updating && _state) {
                    _state->setDimReductionLabelEventKey(labelEventKey());
                }
            });

    // React to external state changes
    if (_state) {
        connect(_state.get(), &MLCoreWidgetState::dimReductionTensorKeyChanged,
                this, [this](QString const & key) {
                    for (int i = 0; i < ui->tensorComboBox->count(); ++i) {
                        QVariant const data = ui->tensorComboBox->itemData(i);
                        if (data.isValid() && data.toString() == key) {
                            ui->tensorComboBox->blockSignals(true);
                            ui->tensorComboBox->setCurrentIndex(i);
                            ui->tensorComboBox->blockSignals(false);
                            _updateTensorInfo();
                            return;
                        }
                    }
                    refreshTensorList();
                });

        connect(_state.get(), &MLCoreWidgetState::dimReductionModelNameChanged,
                this, [this](QString const & name) {
                    for (int i = 0; i < ui->algorithmComboBox->count(); ++i) {
                        if (ui->algorithmComboBox->itemData(i).toString() == name) {
                            ui->algorithmComboBox->blockSignals(true);
                            ui->algorithmComboBox->setCurrentIndex(i);
                            ui->algorithmComboBox->blockSignals(false);
                            return;
                        }
                    }
                });
    }
}

void DimReductionPanel::_populateAlgorithms() {
    _updating = true;
    ui->algorithmComboBox->clear();

    if (isSupervisedMode()) {
        // Supervised algorithms
        auto const names = _registry->getModelNames(
                MLCore::MLTaskType::SupervisedDimensionalityReduction);
        for (auto const & name: names) {
            ui->algorithmComboBox->addItem(
                    QString::fromStdString(name),
                    QString::fromStdString(name));
        }

        // Show/hide param widgets based on the first supervised algorithm
        ui->tsneParamsWidget->setVisible(false);
        ui->robustPcaParamsWidget->setVisible(false);
        ui->larsParamsWidget->setVisible(false);
        if (ui->algorithmComboBox->count() > 0) {
            auto const first_name =
                    ui->algorithmComboBox->currentData().toString().toStdString();
            bool const is_spca = (first_name == "Supervised PCA");
            bool const is_srpca = (first_name == "Supervised Robust PCA");
            bool const is_lars = (first_name == "LARS Projection");
            ui->pcaParamsWidget->setVisible(is_spca);
            ui->robustPcaParamsWidget->setVisible(is_srpca);
            ui->larsParamsWidget->setVisible(is_lars);
            if (is_lars) {
                _updateLarsRegularizationUI(ui->larsRegTypeCombo->currentIndex());
            }

            // Hide class name fields for non-discriminative algorithms
            ui->positiveClassLabel->setVisible(!is_spca && !is_srpca);
            ui->positiveClassEdit->setVisible(!is_spca && !is_srpca);
            ui->negativeClassLabel->setVisible(!is_spca && !is_srpca);
            ui->negativeClassEdit->setVisible(!is_spca && !is_srpca);
        } else {
            ui->pcaParamsWidget->setVisible(false);
        }
    } else {
        // Unsupervised algorithms
        auto const names = _registry->getModelNames(
                MLCore::MLTaskType::DimensionalityReduction);
        for (auto const & name: names) {
            ui->algorithmComboBox->addItem(
                    QString::fromStdString(name),
                    QString::fromStdString(name));
        }

        if (ui->algorithmComboBox->count() > 0) {
            // Set initial parameter widget visibility
            auto const first_name =
                    ui->algorithmComboBox->currentData().toString().toStdString();
            ui->pcaParamsWidget->setVisible(first_name == "PCA");
            ui->tsneParamsWidget->setVisible(first_name == "t-SNE");
            ui->robustPcaParamsWidget->setVisible(first_name == "Robust PCA");
            ui->larsParamsWidget->setVisible(false);
        }
    }

    if (ui->algorithmComboBox->count() > 0) {
        ui->algorithmComboBox->setCurrentIndex(0);
    }

    _updating = false;
}

void DimReductionPanel::_updateTensorInfo() {
    std::string const key = selectedTensorKey();

    if (key.empty() || !_data_manager) {
        ui->tensorInfoLabel->setText(QStringLiteral("No tensor selected"));
        return;
    }

    auto tensor = _data_manager->getData<TensorData>(key);
    if (!tensor) {
        ui->tensorInfoLabel->setText(
                QStringLiteral("<span style='color: red;'>Tensor \"%1\" not found</span>")
                        .arg(QString::fromStdString(key)));
        return;
    }

    QString row_type_str;
    switch (tensor->rowType()) {
        case RowType::TimeFrameIndex:
            row_type_str = QStringLiteral("TimeFrameIndex");
            break;
        case RowType::Interval:
            row_type_str = QStringLiteral("Interval");
            break;
        case RowType::Ordinal:
            row_type_str = QStringLiteral("Ordinal");
            break;
    }

    ui->tensorInfoLabel->setStyleSheet(QString{});
    ui->tensorInfoLabel->setText(
            QStringLiteral("<b>Shape:</b> %1 x %2 &nbsp; <b>Rows:</b> %3")
                    .arg(tensor->numRows())
                    .arg(tensor->numColumns())
                    .arg(row_type_str));

    // Clamp n_components to the tensor's column count
    auto const max_components = static_cast<int>(tensor->numColumns());
    ui->nComponentsSpinBox->setMaximum(max_components);
    if (ui->nComponentsSpinBox->value() > max_components) {
        ui->nComponentsSpinBox->setValue(std::min(2, max_components));
    }
}

void DimReductionPanel::_registerDataManagerObserver() {
    if (!_data_manager) {
        return;
    }
    _dm_observer_id = _data_manager->addObserver(
            [this]() {
                refreshTensorList();
                if (isSupervisedMode()) {
                    refreshLabelSources();
                }
            });

    // Register group observer for group-based label modes
    auto * group_mgr = _data_manager->getEntityGroupManager();
    if (group_mgr) {
        _group_observer_id = group_mgr->getGroupObservers().addObserver(
                [this]() {
                    if (isSupervisedMode()) {
                        _refreshGroupCombo();
                        _refreshDataGroupCombo();
                    }
                });
    }
}

void DimReductionPanel::_restoreFromState() {
    if (!_state) {
        return;
    }

    _updating = true;

    // Restore supervised mode first (affects algorithm list)
    bool const supervised = _state->dimReductionSupervised();
    ui->supervisedRadio->setChecked(supervised);
    ui->unsupervisedRadio->setChecked(!supervised);
    _updateSupervisedVisibility(supervised);
    _populateAlgorithms();

    // Restore tensor key
    auto const & key = _state->dimReductionTensorKey();
    if (!key.empty()) {
        for (int i = 0; i < ui->tensorComboBox->count(); ++i) {
            QVariant const data = ui->tensorComboBox->itemData(i);
            if (data.isValid() && data.toString().toStdString() == key) {
                ui->tensorComboBox->setCurrentIndex(i);
                break;
            }
        }
    }

    // Restore algorithm
    auto const & model_name = _state->dimReductionModelName();
    if (!model_name.empty()) {
        for (int i = 0; i < ui->algorithmComboBox->count(); ++i) {
            if (ui->algorithmComboBox->itemData(i).toString().toStdString() == model_name) {
                ui->algorithmComboBox->setCurrentIndex(i);
                break;
            }
        }
    }

    // Restore parameters
    ui->nComponentsSpinBox->setValue(_state->dimReductionNComponents());
    ui->zscoreCheckBox->setChecked(_state->dimReductionZscoreNormalize());

    auto const & output_key = _state->dimReductionOutputKey();
    if (!output_key.empty()) {
        ui->outputKeyLineEdit->setText(QString::fromStdString(output_key));
    }

    // Restore supervised label config
    if (supervised) {
        refreshLabelSources();

        auto const & src_type = _state->dimReductionLabelSourceType();
        for (int i = 0; i < ui->labelSourceCombo->count(); ++i) {
            if (ui->labelSourceCombo->itemData(i).toString().toStdString() == src_type) {
                ui->labelSourceCombo->setCurrentIndex(i);
                ui->labelStack->setCurrentIndex(i);
                break;
            }
        }

        // Interval mode
        auto const & interval_key = _state->dimReductionLabelIntervalKey();
        if (!interval_key.empty()) {
            for (int i = 0; i < ui->intervalKeyCombo->count(); ++i) {
                if (ui->intervalKeyCombo->itemData(i).toString().toStdString() == interval_key) {
                    ui->intervalKeyCombo->setCurrentIndex(i);
                    break;
                }
            }
        }
        ui->positiveClassEdit->setText(
                QString::fromStdString(_state->dimReductionLabelPositiveClass()));
        ui->negativeClassEdit->setText(
                QString::fromStdString(_state->dimReductionLabelNegativeClass()));

        // Event mode
        auto const & event_key = _state->dimReductionLabelEventKey();
        if (!event_key.empty()) {
            for (int i = 0; i < ui->eventKeyCombo->count(); ++i) {
                if (ui->eventKeyCombo->itemData(i).toString().toStdString() == event_key) {
                    ui->eventKeyCombo->setCurrentIndex(i);
                    break;
                }
            }
        }

        // Group mode
        _selected_group_ids.clear();
        for (auto const gid: _state->dimReductionLabelGroupIds()) {
            _selected_group_ids.push_back(gid);
        }
        _rebuildGroupClassList();

        // Data key
        auto const & data_key = _state->dimReductionLabelDataKey();
        if (!data_key.empty()) {
            for (int i = 0; i < ui->dataKeyCombo->count(); ++i) {
                if (ui->dataKeyCombo->itemData(i).toString().toStdString() == data_key) {
                    ui->dataKeyCombo->setCurrentIndex(i);
                    break;
                }
            }
        }
    }

    _updating = false;
    _updateTensorInfo();
}

void DimReductionPanel::_syncToState() {
    if (!_state || _updating) {
        return;
    }

    _state->setDimReductionTensorKey(selectedTensorKey());
    _state->setDimReductionModelName(selectedAlgorithmName());
    _state->setDimReductionOutputKey(outputKey());
    _state->setDimReductionNComponents(nComponents());
    _state->setDimReductionZscoreNormalize(zscoreNormalize());
    _state->setDimReductionSupervised(isSupervisedMode());

    if (isSupervisedMode()) {
        _state->setDimReductionLabelSourceType(labelSourceType());
        _state->setDimReductionLabelIntervalKey(labelIntervalKey());
        _state->setDimReductionLabelPositiveClass(labelPositiveClassName());
        _state->setDimReductionLabelNegativeClass(labelNegativeClassName());
        _state->setDimReductionLabelEventKey(labelEventKey());
        _state->setDimReductionLabelDataKey(labelDataKey());
        _syncGroupIdsToState();
    }
}

void DimReductionPanel::_updateOutputKeyFromInput() {
    if (_updating) {
        return;
    }

    std::string const key = selectedTensorKey();
    if (key.empty()) {
        return;
    }

    std::string const algo = selectedAlgorithmName();
    std::string suffix = "reduced";

    if (isSupervisedMode() && algo == "Supervised PCA") {
        suffix = "spca";
    } else if (isSupervisedMode() && algo == "Supervised Robust PCA") {
        suffix = "srpca";
    } else if (isSupervisedMode() && algo == "LARS Projection") {
        suffix = "lars";
    } else if (isSupervisedMode()) {
        suffix = "logit";
    } else if (algo == "PCA") {
        suffix = "pca";
    } else if (algo == "t-SNE") {
        suffix = "tsne";
    } else if (algo == "Robust PCA") {
        suffix = "rpca";
    }

    ui->outputKeyLineEdit->setText(
            QString::fromStdString(key + "_" + suffix));
}

// =============================================================================
// Supervised mode helpers
// =============================================================================

void DimReductionPanel::_updateSupervisedVisibility(bool supervised) {
    ui->labelsGroupBox->setVisible(supervised);

    // In supervised mode, hide unsupervised-specific param widgets
    // (the correct widgets will be shown by _populateAlgorithms)
    if (supervised) {
        ui->tsneParamsWidget->setVisible(false);
        ui->robustPcaParamsWidget->setVisible(false);
        ui->larsParamsWidget->setVisible(false);
    }
}

void DimReductionPanel::_populateLabelSourceCombo() {
    ui->labelSourceCombo->clear();
    ui->labelSourceCombo->addItem(QStringLiteral("Interval Series"),
                                  QStringLiteral("intervals"));
    ui->labelSourceCombo->addItem(QStringLiteral("Entity Groups"),
                                  QStringLiteral("groups"));
    ui->labelSourceCombo->addItem(QStringLiteral("Data Entity Groups"),
                                  QStringLiteral("entity_groups"));
    ui->labelSourceCombo->addItem(QStringLiteral("Event Series"),
                                  QStringLiteral("events"));
    ui->labelStack->setCurrentIndex(0);
}

void DimReductionPanel::_refreshIntervalCombo() {
    if (!_data_manager) {
        return;
    }

    QString current;
    if (ui->intervalKeyCombo->currentIndex() >= 0) {
        QVariant const d = ui->intervalKeyCombo->itemData(ui->intervalKeyCombo->currentIndex());
        if (d.isValid()) {
            current = d.toString();
        }
    }

    ui->intervalKeyCombo->blockSignals(true);
    ui->intervalKeyCombo->clear();
    ui->intervalKeyCombo->addItem(QString{});// empty sentinel

    auto keys = _data_manager->getKeys<DigitalIntervalSeries>();
    std::sort(keys.begin(), keys.end());
    for (auto const & k: keys) {
        ui->intervalKeyCombo->addItem(
                QString::fromStdString(k), QString::fromStdString(k));
    }

    // Restore
    for (int i = 0; i < ui->intervalKeyCombo->count(); ++i) {
        if (ui->intervalKeyCombo->itemData(i).toString() == current) {
            ui->intervalKeyCombo->setCurrentIndex(i);
            break;
        }
    }
    ui->intervalKeyCombo->blockSignals(false);
}

void DimReductionPanel::_refreshGroupCombo() {
    if (!_data_manager) {
        return;
    }

    ui->groupCombo->blockSignals(true);
    ui->groupCombo->clear();

    auto * group_mgr = _data_manager->getEntityGroupManager();
    if (group_mgr) {
        auto const descriptors = group_mgr->getAllGroupDescriptors();
        for (auto const & desc: descriptors) {
            QString const display = QStringLiteral("%1 (%2 entities)")
                                            .arg(QString::fromStdString(desc.name))
                                            .arg(desc.entity_count);
            ui->groupCombo->addItem(
                    display,
                    QVariant::fromValue(static_cast<qulonglong>(desc.id)));
        }
    }
    ui->groupCombo->blockSignals(false);
}

void DimReductionPanel::_refreshDataKeyCombo() {
    if (!_data_manager) {
        return;
    }

    QString current;
    if (ui->dataKeyCombo->currentIndex() >= 0) {
        current = ui->dataKeyCombo->currentData().toString();
    }

    ui->dataKeyCombo->blockSignals(true);
    ui->dataKeyCombo->clear();
    ui->dataKeyCombo->addItem(QString{});

    auto const all_keys = _data_manager->getAllKeys();
    for (auto const & k: all_keys) {
        ui->dataKeyCombo->addItem(
                QString::fromStdString(k), QString::fromStdString(k));
    }

    for (int i = 0; i < ui->dataKeyCombo->count(); ++i) {
        if (ui->dataKeyCombo->itemData(i).toString() == current) {
            ui->dataKeyCombo->setCurrentIndex(i);
            break;
        }
    }
    ui->dataKeyCombo->blockSignals(false);
}

void DimReductionPanel::_refreshDataGroupCombo() {
    // Same as group combo for now
    if (!_data_manager) {
        return;
    }

    ui->dataGroupCombo->blockSignals(true);
    ui->dataGroupCombo->clear();

    auto * group_mgr = _data_manager->getEntityGroupManager();
    if (group_mgr) {
        auto const descriptors = group_mgr->getAllGroupDescriptors();
        for (auto const & desc: descriptors) {
            QString const display = QStringLiteral("%1 (%2 entities)")
                                            .arg(QString::fromStdString(desc.name))
                                            .arg(desc.entity_count);
            ui->dataGroupCombo->addItem(
                    display,
                    QVariant::fromValue(static_cast<qulonglong>(desc.id)));
        }
    }
    ui->dataGroupCombo->blockSignals(false);
}

void DimReductionPanel::_refreshEventCombo() {
    if (!_data_manager) {
        return;
    }

    QString current;
    if (ui->eventKeyCombo->currentIndex() >= 0) {
        QVariant const d = ui->eventKeyCombo->itemData(ui->eventKeyCombo->currentIndex());
        if (d.isValid()) {
            current = d.toString();
        }
    }

    ui->eventKeyCombo->blockSignals(true);
    ui->eventKeyCombo->clear();
    ui->eventKeyCombo->addItem(QString{});

    auto keys = _data_manager->getKeys<DigitalEventSeries>();
    std::sort(keys.begin(), keys.end());
    for (auto const & k: keys) {
        ui->eventKeyCombo->addItem(
                QString::fromStdString(k), QString::fromStdString(k));
    }

    for (int i = 0; i < ui->eventKeyCombo->count(); ++i) {
        if (ui->eventKeyCombo->itemData(i).toString() == current) {
            ui->eventKeyCombo->setCurrentIndex(i);
            break;
        }
    }
    ui->eventKeyCombo->blockSignals(false);
}

void DimReductionPanel::_rebuildGroupClassList() {
    ui->groupClassList->clear();

    if (!_data_manager) {
        return;
    }
    auto * group_mgr = _data_manager->getEntityGroupManager();
    if (!group_mgr) {
        return;
    }

    for (auto const gid: _selected_group_ids) {
        auto const desc = group_mgr->getGroupDescriptor(gid);
        QString text;
        if (desc) {
            text = QStringLiteral("Class %1: \"%2\" (%3 entities)")
                           .arg(ui->groupClassList->count())
                           .arg(QString::fromStdString(desc->name))
                           .arg(desc->entity_count);
        } else {
            text = QStringLiteral("Class %1: <deleted group %2>")
                           .arg(ui->groupClassList->count())
                           .arg(gid);
        }
        auto * item = new QListWidgetItem(text);
        item->setData(Qt::UserRole, QVariant::fromValue(static_cast<qulonglong>(gid)));
        ui->groupClassList->addItem(item);
    }
}

void DimReductionPanel::_rebuildDataGroupClassList() {
    ui->dataGroupClassList->clear();

    if (!_data_manager) {
        return;
    }
    auto * group_mgr = _data_manager->getEntityGroupManager();
    if (!group_mgr) {
        return;
    }

    for (auto const gid: _selected_data_group_ids) {
        auto const desc = group_mgr->getGroupDescriptor(gid);
        QString text;
        if (desc) {
            text = QStringLiteral("Class %1: \"%2\" (%3 entities)")
                           .arg(ui->dataGroupClassList->count())
                           .arg(QString::fromStdString(desc->name))
                           .arg(desc->entity_count);
        } else {
            text = QStringLiteral("Class %1: <deleted group %2>")
                           .arg(ui->dataGroupClassList->count())
                           .arg(gid);
        }
        auto * item = new QListWidgetItem(text);
        item->setData(Qt::UserRole, QVariant::fromValue(static_cast<qulonglong>(gid)));
        ui->dataGroupClassList->addItem(item);
    }
}

void DimReductionPanel::_syncGroupIdsToState() {
    if (!_state) {
        return;
    }

    auto const source = labelSourceType();
    if (source == "groups") {
        _state->setDimReductionLabelGroupIds(_selected_group_ids);
    } else if (source == "entity_groups") {
        _state->setDimReductionLabelGroupIds(_selected_data_group_ids);
    }
}
