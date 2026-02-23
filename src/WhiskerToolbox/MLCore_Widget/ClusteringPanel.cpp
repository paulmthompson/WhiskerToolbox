#include "ClusteringPanel.hpp"

#include "MLCoreWidgetState.hpp"
#include "ui_ClusteringPanel.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/Tensors/TensorData.hpp"
#include "MLCore/models/MLModelParameters.hpp"
#include "MLCore/models/MLModelRegistry.hpp"
#include "MLCore/models/MLTaskType.hpp"

#include <QTimer>
#include <QVariant>

#include <algorithm>

// =============================================================================
// Page index constants
// =============================================================================

namespace {

// Parameter stack page indices — must match the order in ClusteringPanel.ui
constexpr int kPageEmpty = 0;
constexpr int kPageKMeans = 1;
constexpr int kPageDBSCAN = 2;
constexpr int kPageGMM = 3;

}// namespace

// =============================================================================
// Construction / destruction
// =============================================================================

ClusteringPanel::ClusteringPanel(
        std::shared_ptr<MLCoreWidgetState> state,
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent)
    : QWidget(parent),
      ui(new Ui::ClusteringPanel),
      _state(std::move(state)),
      _data_manager(std::move(data_manager)),
      _registry(std::make_unique<MLCore::MLModelRegistry>()) {
    ui->setupUi(this);
    _populateAlgorithms();
    _setupConnections();
    _registerDataManagerObserver();

    // Initial population
    refreshTensorList();

    // Restore from state
    _restoreFromState();
}

ClusteringPanel::~ClusteringPanel() {
    if (_dm_observer_id >= 0 && _data_manager) {
        _data_manager->removeObserver(_dm_observer_id);
    }
    delete ui;
}

// =============================================================================
// Public API
// =============================================================================

std::string ClusteringPanel::selectedTensorKey() const {
    int const idx = ui->tensorComboBox->currentIndex();
    if (idx < 0) {
        return {};
    }
    QVariant data = ui->tensorComboBox->itemData(idx);
    if (data.isValid()) {
        return data.toString().toStdString();
    }
    return ui->tensorComboBox->currentText().toStdString();
}

std::string ClusteringPanel::selectedAlgorithmName() const {
    int const idx = ui->algorithmComboBox->currentIndex();
    if (idx < 0) {
        return {};
    }
    return ui->algorithmComboBox->currentData().toString().toStdString();
}

std::unique_ptr<MLCore::MLModelParametersBase> ClusteringPanel::currentParameters() const {
    auto const name = selectedAlgorithmName();
    int const page = _pageIndexForModel(name);

    switch (page) {
        case kPageKMeans: {
            auto params = std::make_unique<MLCore::KMeansParameters>();
            params->k = static_cast<std::size_t>(ui->kmKSpinBox->value());
            params->max_iterations = static_cast<std::size_t>(ui->kmMaxIterSpinBox->value());
            int const seed_val = ui->kmSeedSpinBox->value();
            if (seed_val >= 0) {
                params->seed = static_cast<std::size_t>(seed_val);
            }
            return params;
        }
        case kPageDBSCAN: {
            auto params = std::make_unique<MLCore::DBSCANParameters>();
            params->epsilon = ui->dbEpsilonSpinBox->value();
            params->min_points = static_cast<std::size_t>(ui->dbMinPointsSpinBox->value());
            return params;
        }
        case kPageGMM: {
            auto params = std::make_unique<MLCore::GMMParameters>();
            params->k = static_cast<std::size_t>(ui->gmmKSpinBox->value());
            params->max_iterations = static_cast<std::size_t>(ui->gmmMaxIterSpinBox->value());
            int const seed_val = ui->gmmSeedSpinBox->value();
            if (seed_val >= 0) {
                params->seed = static_cast<std::size_t>(seed_val);
            }
            return params;
        }
        default:
            return nullptr;
    }
}

std::string ClusteringPanel::outputPrefix() const {
    return ui->prefixLineEdit->text().toStdString();
}

bool ClusteringPanel::writeIntervals() const {
    return ui->writeIntervalsCheckBox->isChecked();
}

bool ClusteringPanel::writeProbabilities() const {
    return ui->writeProbabilitiesCheckBox->isChecked();
}

bool ClusteringPanel::zscoreNormalize() const {
    return ui->zscoreCheckBox->isChecked();
}

bool ClusteringPanel::hasValidConfiguration() const {
    return !selectedTensorKey().empty() && !selectedAlgorithmName().empty();
}

// =============================================================================
// Public slots
// =============================================================================

void ClusteringPanel::refreshTensorList() {
    if (!_data_manager) {
        return;
    }

    QString current_selection;
    int const cur_idx = ui->tensorComboBox->currentIndex();
    if (cur_idx >= 0) {
        QVariant data = ui->tensorComboBox->itemData(cur_idx);
        if (data.isValid()) {
            current_selection = data.toString();
        } else {
            current_selection = ui->tensorComboBox->currentText();
        }
    }

    ui->tensorComboBox->blockSignals(true);
    ui->tensorComboBox->clear();

    // Add empty sentinel item
    ui->tensorComboBox->addItem(QString{});

    auto tensor_keys = _data_manager->getKeys<TensorData>();
    std::sort(tensor_keys.begin(), tensor_keys.end());

    for (auto const & key: tensor_keys) {
        auto tensor = _data_manager->getData<TensorData>(key);
        if (tensor) {
            QString display = QStringLiteral("%1 (%2×%3)")
                                      .arg(QString::fromStdString(key))
                                      .arg(tensor->numRows())
                                      .arg(tensor->numColumns());
            ui->tensorComboBox->addItem(display, QString::fromStdString(key));
        } else {
            ui->tensorComboBox->addItem(QString::fromStdString(key),
                                        QString::fromStdString(key));
        }
    }

    // Restore previous selection if still available
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

void ClusteringPanel::_onTensorComboChanged(int index) {
    QString key_str;
    if (index >= 0) {
        QVariant data = ui->tensorComboBox->itemData(index);
        if (data.isValid()) {
            key_str = data.toString();
        } else {
            key_str = ui->tensorComboBox->itemText(index);
        }
    }

    std::string key = key_str.toStdString();

    if (_state) {
        _state->setClusteringTensorKey(key);
    }

    _updateTensorInfo();
    emit tensorSelectionChanged(key_str);
}

void ClusteringPanel::_onAlgorithmChanged(int index) {
    if (_updating) {
        return;
    }

    if (index < 0) {
        ui->parameterStack->setCurrentIndex(kPageEmpty);
        if (_state) {
            _state->setClusteringModelName({});
        }
        emit algorithmChanged({});
        return;
    }

    auto const name = ui->algorithmComboBox->currentData().toString().toStdString();
    _switchParameterPage(name);

    if (_state) {
        _state->setClusteringModelName(name);
    }

    emit algorithmChanged(QString::fromStdString(name));
}

void ClusteringPanel::_onFitClicked() {
    _syncToState();
    emit fitRequested();
}

void ClusteringPanel::_onParameterValueChanged() {
    if (!_updating) {
        _syncToState();
        emit parametersChanged();
    }
}

// =============================================================================
// Setup helpers
// =============================================================================

void ClusteringPanel::_setupConnections() {
    // Tensor combo
    connect(ui->tensorComboBox, &QComboBox::currentIndexChanged,
            this, &ClusteringPanel::_onTensorComboChanged);

    connect(ui->refreshButton, &QPushButton::clicked,
            this, &ClusteringPanel::refreshTensorList);

    // Algorithm combo
    connect(ui->algorithmComboBox, &QComboBox::currentIndexChanged,
            this, &ClusteringPanel::_onAlgorithmChanged);

    // Fit button
    connect(ui->fitButton, &QPushButton::clicked,
            this, &ClusteringPanel::_onFitClicked);

    // Z-score checkbox
    connect(ui->zscoreCheckBox, &QCheckBox::toggled,
            this, [this](bool checked) {
                if (!_updating && _state) {
                    _state->setClusteringZscoreNormalize(checked);
                }
                emit parametersChanged();
            });

    // Output config
    connect(ui->prefixLineEdit, &QLineEdit::textChanged,
            this, [this](QString const & text) {
                if (!_updating && _state) {
                    _state->setClusteringOutputPrefix(text.toStdString());
                }
            });

    connect(ui->writeIntervalsCheckBox, &QCheckBox::toggled,
            this, [this](bool checked) {
                if (!_updating && _state) {
                    _state->setClusteringWriteIntervals(checked);
                }
            });

    connect(ui->writeProbabilitiesCheckBox, &QCheckBox::toggled,
            this, [this](bool checked) {
                if (!_updating && _state) {
                    _state->setClusteringWriteProbabilities(checked);
                }
            });

    // K-Means parameter changes
    connect(ui->kmKSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ClusteringPanel::_onParameterValueChanged);
    connect(ui->kmMaxIterSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ClusteringPanel::_onParameterValueChanged);
    connect(ui->kmSeedSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ClusteringPanel::_onParameterValueChanged);

    // DBSCAN parameter changes
    connect(ui->dbEpsilonSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ClusteringPanel::_onParameterValueChanged);
    connect(ui->dbMinPointsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ClusteringPanel::_onParameterValueChanged);

    // GMM parameter changes
    connect(ui->gmmKSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ClusteringPanel::_onParameterValueChanged);
    connect(ui->gmmMaxIterSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ClusteringPanel::_onParameterValueChanged);
    connect(ui->gmmSeedSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ClusteringPanel::_onParameterValueChanged);

    // React to external state changes (e.g., from JSON restore)
    if (_state) {
        connect(_state.get(), &MLCoreWidgetState::clusteringTensorKeyChanged,
                this, [this](QString const & key) {
                    for (int i = 0; i < ui->tensorComboBox->count(); ++i) {
                        QVariant data = ui->tensorComboBox->itemData(i);
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

        connect(_state.get(), &MLCoreWidgetState::clusteringModelNameChanged,
                this, [this](QString const & name) {
                    for (int i = 0; i < ui->algorithmComboBox->count(); ++i) {
                        if (ui->algorithmComboBox->itemData(i).toString() == name) {
                            ui->algorithmComboBox->blockSignals(true);
                            ui->algorithmComboBox->setCurrentIndex(i);
                            ui->algorithmComboBox->blockSignals(false);
                            _switchParameterPage(name.toStdString());
                            return;
                        }
                    }
                });
    }
}

void ClusteringPanel::_populateAlgorithms() {
    _updating = true;
    ui->algorithmComboBox->clear();

    auto const names = _registry->getModelNames(MLCore::MLTaskType::Clustering);
    for (auto const & name: names) {
        ui->algorithmComboBox->addItem(
                QString::fromStdString(name),
                QString::fromStdString(name));
    }

    if (ui->algorithmComboBox->count() > 0) {
        ui->algorithmComboBox->setCurrentIndex(0);
        auto const first_name = ui->algorithmComboBox->itemData(0).toString().toStdString();
        _switchParameterPage(first_name);
    } else {
        ui->parameterStack->setCurrentIndex(kPageEmpty);
    }

    _updating = false;
}

void ClusteringPanel::_updateTensorInfo() {
    std::string key = selectedTensorKey();

    if (key.empty() || !_data_manager) {
        ui->tensorInfoLabel->setText(QStringLiteral("No tensor selected"));
        ui->columnListLabel->setText(QString{});
        ui->validationLabel->setText(QString{});
        return;
    }

    auto tensor = _data_manager->getData<TensorData>(key);
    if (!tensor) {
        ui->tensorInfoLabel->setText(
                QStringLiteral("<span style='color: red;'>Tensor \"%1\" not found</span>")
                        .arg(QString::fromStdString(key)));
        ui->columnListLabel->setText(QString{});
        ui->validationLabel->setText(QString{});
        return;
    }

    // --- Row type ---
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

    // Shape info
    QString info = QStringLiteral("<b>Shape:</b> %1 rows × %2 columns<br>"
                                  "<b>Row type:</b> %3")
                           .arg(tensor->numRows())
                           .arg(tensor->numColumns())
                           .arg(row_type_str);

    // Time frame info (if temporal)
    if (tensor->rowType() != RowType::Ordinal) {
        TimeKey time_key = _data_manager->getTimeKey(key);
        if (!time_key.empty()) {
            auto time_frame = _data_manager->getTime(time_key);
            QString tf_info = QStringLiteral("<br><b>Time frame:</b> %1")
                                      .arg(QString::fromStdString(time_key.str()));
            if (time_frame) {
                tf_info += QStringLiteral(" (%1 total frames)")
                                   .arg(time_frame->getTotalFrameCount());
            }
            info += tf_info;
        }
    }

    ui->tensorInfoLabel->setText(info);

    // --- Column names ---
    if (tensor->hasNamedColumns()) {
        auto const & names = tensor->columnNames();
        QStringList col_list;
        col_list.reserve(static_cast<int>(names.size()));
        for (auto const & name: names) {
            col_list.append(QString::fromStdString(name));
        }

        constexpr int max_display_cols = 20;
        QString col_text;
        if (col_list.size() <= max_display_cols) {
            col_text = QStringLiteral("<b>Columns:</b> %1").arg(col_list.join(", "));
        } else {
            QStringList truncated = col_list.mid(0, max_display_cols);
            col_text = QStringLiteral("<b>Columns:</b> %1, ... (+%2 more)")
                               .arg(truncated.join(", "))
                               .arg(col_list.size() - max_display_cols);
        }
        ui->columnListLabel->setText(col_text);
    } else {
        ui->columnListLabel->setText(
                QStringLiteral("<b>Columns:</b> %1 unnamed columns")
                        .arg(tensor->numColumns()));
    }

    // --- Validation ---
    QString validation_msg;
    if (tensor->numRows() == 0) {
        validation_msg = QStringLiteral(
                "<span style='color: orange;'>Warning: tensor has 0 rows</span>");
    } else if (tensor->numColumns() == 0) {
        validation_msg = QStringLiteral(
                "<span style='color: orange;'>Warning: tensor has 0 columns</span>");
    }
    ui->validationLabel->setText(validation_msg);
}

void ClusteringPanel::_switchParameterPage(std::string const & model_name) {
    int const page = _pageIndexForModel(model_name);
    ui->parameterStack->setCurrentIndex(page);
}

int ClusteringPanel::_pageIndexForModel(std::string const & name) const {
    if (name == "K-Means") {
        return kPageKMeans;
    }
    if (name == "DBSCAN") {
        return kPageDBSCAN;
    }
    if (name == "Gaussian Mixture Model") {
        return kPageGMM;
    }
    return kPageEmpty;
}

void ClusteringPanel::_registerDataManagerObserver() {
    if (!_data_manager) {
        return;
    }

    _dm_observer_id = _data_manager->addObserver([this]() {
        QTimer::singleShot(0, this, &ClusteringPanel::refreshTensorList);
    });
}

void ClusteringPanel::_restoreFromState() {
    if (!_state) {
        return;
    }

    _updating = true;

    // Restore tensor selection
    if (!_state->clusteringTensorKey().empty()) {
        QString const key = QString::fromStdString(_state->clusteringTensorKey());
        for (int i = 0; i < ui->tensorComboBox->count(); ++i) {
            if (ui->tensorComboBox->itemData(i).toString() == key) {
                ui->tensorComboBox->setCurrentIndex(i);
                break;
            }
        }
    }

    // Restore algorithm selection
    if (!_state->clusteringModelName().empty()) {
        QString const name = QString::fromStdString(_state->clusteringModelName());
        for (int i = 0; i < ui->algorithmComboBox->count(); ++i) {
            if (ui->algorithmComboBox->itemData(i).toString() == name) {
                ui->algorithmComboBox->setCurrentIndex(i);
                _switchParameterPage(name.toStdString());
                break;
            }
        }
    }

    // Restore output config
    if (!_state->clusteringOutputPrefix().empty()) {
        ui->prefixLineEdit->setText(
                QString::fromStdString(_state->clusteringOutputPrefix()));
    }

    ui->writeIntervalsCheckBox->setChecked(_state->clusteringWriteIntervals());
    ui->writeProbabilitiesCheckBox->setChecked(_state->clusteringWriteProbabilities());
    ui->zscoreCheckBox->setChecked(_state->clusteringZscoreNormalize());

    _updating = false;

    _updateTensorInfo();
}

void ClusteringPanel::_syncToState() {
    if (!_state || _updating) {
        return;
    }

    _state->setClusteringTensorKey(selectedTensorKey());
    _state->setClusteringModelName(selectedAlgorithmName());
    _state->setClusteringOutputPrefix(outputPrefix());
    _state->setClusteringWriteIntervals(writeIntervals());
    _state->setClusteringWriteProbabilities(writeProbabilities());
    _state->setClusteringZscoreNormalize(zscoreNormalize());
}
