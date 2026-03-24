/**
 * @file DimReductionPanel.cpp
 * @brief Implementation of the dimensionality reduction panel
 */

#include "DimReductionPanel.hpp"

#include "Core/MLCoreWidgetState.hpp"
#include "ui_DimReductionPanel.h"

#include "DataManager/DataManager.hpp"
#include "MLCore/models/MLModelParameters.hpp"
#include "MLCore/models/MLModelRegistry.hpp"
#include "MLCore/models/MLTaskType.hpp"
#include "Tensors/TensorData.hpp"

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
    _setupConnections();
    _registerDataManagerObserver();

    refreshTensorList();
    _restoreFromState();
}

DimReductionPanel::~DimReductionPanel() {
    if (_dm_observer_id >= 0 && _data_manager) {
        _data_manager->removeObserver(_dm_observer_id);
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

bool DimReductionPanel::scaleFeatures() const {
    return ui->scaleCheckBox->isChecked();
}

bool DimReductionPanel::zscoreNormalize() const {
    return ui->zscoreCheckBox->isChecked();
}

std::string DimReductionPanel::outputKey() const {
    return ui->outputKeyLineEdit->text().toStdString();
}

bool DimReductionPanel::hasValidConfiguration() const {
    return !selectedTensorKey().empty() &&
           !selectedAlgorithmName().empty() &&
           !outputKey().empty();
}

std::unique_ptr<MLCore::MLModelParametersBase> DimReductionPanel::currentParameters() const {
    auto const name = selectedAlgorithmName();

    // Currently only PCA is supported
    if (name == "PCA") {
        auto params = std::make_unique<MLCore::PCAParameters>();
        params->n_components = static_cast<std::size_t>(nComponents());
        params->scale = scaleFeatures();
        return params;
    }

    return nullptr;
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
}

void DimReductionPanel::_onRunClicked() {
    _syncToState();
    emit runRequested();
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

    connect(ui->scaleCheckBox, &QCheckBox::toggled,
            this, [this](bool checked) {
                if (!_updating && _state) {
                    _state->setDimReductionScale(checked);
                }
            });

    connect(ui->outputKeyLineEdit, &QLineEdit::textChanged,
            this, [this](QString const & text) {
                if (!_updating && _state) {
                    _state->setDimReductionOutputKey(text.toStdString());
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

    auto const names = _registry->getModelNames(MLCore::MLTaskType::DimensionalityReduction);
    for (auto const & name: names) {
        ui->algorithmComboBox->addItem(
                QString::fromStdString(name),
                QString::fromStdString(name));
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
            [this]() { refreshTensorList(); });
}

void DimReductionPanel::_restoreFromState() {
    if (!_state) {
        return;
    }

    _updating = true;

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
    ui->scaleCheckBox->setChecked(_state->dimReductionScale());
    ui->zscoreCheckBox->setChecked(_state->dimReductionZscoreNormalize());

    auto const & output_key = _state->dimReductionOutputKey();
    if (!output_key.empty()) {
        ui->outputKeyLineEdit->setText(QString::fromStdString(output_key));
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
    _state->setDimReductionScale(scaleFeatures());
    _state->setDimReductionZscoreNormalize(zscoreNormalize());
}

void DimReductionPanel::_updateOutputKeyFromInput() {
    if (_updating) {
        return;
    }

    std::string const key = selectedTensorKey();
    if (key.empty()) {
        return;
    }

    // Auto-generate output key: "{input_key}_pca"
    std::string const algo = selectedAlgorithmName();
    std::string suffix = "reduced";
    if (algo == "PCA") {
        suffix = "pca";
    }

    ui->outputKeyLineEdit->setText(
            QString::fromStdString(key + "_" + suffix));
}
