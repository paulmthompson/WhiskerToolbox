#include "ModelConfigPanel.hpp"

#include "MLCoreWidgetState.hpp"
#include "ui_ModelConfigPanel.h"

#include "MLCore/models/MLModelParameters.hpp"
#include "MLCore/models/MLModelRegistry.hpp"
#include "MLCore/models/MLTaskType.hpp"
#include "MLCore/preprocessing/ClassBalancing.hpp"

#include <QVariant>

#include <optional>

// =============================================================================
// Page index constants
// =============================================================================

namespace {

// Parameter stack page indices — must match the order in the .ui file
constexpr int kPageEmpty = 0;
constexpr int kPageRandomForest = 1;
constexpr int kPageNaiveBayes = 2;
constexpr int kPageLogisticRegression = 3;
constexpr int kPageKMeans = 4;
constexpr int kPageDBSCAN = 5;
constexpr int kPageGMM = 6;

}// namespace

// =============================================================================
// Construction / destruction
// =============================================================================

ModelConfigPanel::ModelConfigPanel(
        std::shared_ptr<MLCoreWidgetState> state,
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent)
    : QWidget(parent),
      ui(new Ui::ModelConfigPanel),
      _state(std::move(state)),
      _data_manager(std::move(data_manager)),
      _registry(std::make_unique<MLCore::MLModelRegistry>()) {
    ui->setupUi(this);
    _populateTaskTypes();
    _populateBalancingStrategies();
    _populateAlgorithms();
    _setupConnections();
    _updateBalancingVisibility();
    _restoreFromState();
}

ModelConfigPanel::~ModelConfigPanel() {
    delete ui;
}

// =============================================================================
// Public API
// =============================================================================

std::string ModelConfigPanel::selectedModelName() const {
    int const idx = ui->algorithmComboBox->currentIndex();
    if (idx < 0) {
        return {};
    }
    return ui->algorithmComboBox->currentData().toString().toStdString();
}

std::unique_ptr<MLCore::MLModelParametersBase> ModelConfigPanel::currentParameters() const {
    auto const name = selectedModelName();
    int const page = _pageIndexForModel(name);

    switch (page) {
        case kPageRandomForest: {
            auto params = std::make_unique<MLCore::RandomForestParameters>();
            params->num_trees = ui->rfNumTreesSpinBox->value();
            params->minimum_leaf_size = ui->rfMinLeafSpinBox->value();
            params->maximum_depth = ui->rfMaxDepthSpinBox->value();
            return params;
        }
        case kPageNaiveBayes: {
            auto params = std::make_unique<MLCore::NaiveBayesParameters>();
            params->epsilon = ui->nbEpsilonSpinBox->value();
            return params;
        }
        case kPageLogisticRegression: {
            auto params = std::make_unique<MLCore::LogisticRegressionParameters>();
            params->lambda = ui->lrLambdaSpinBox->value();
            params->max_iterations = static_cast<std::size_t>(ui->lrMaxIterSpinBox->value());
            return params;
        }
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

bool ModelConfigPanel::isBalancingEnabled() const {
    return ui->enableBalancingCheckBox->isChecked();
}

MLCore::BalancingStrategy ModelConfigPanel::balancingStrategy() const {
    int const idx = ui->strategyComboBox->currentIndex();
    if (idx == 1) {
        return MLCore::BalancingStrategy::Oversample;
    }
    return MLCore::BalancingStrategy::Subsample;
}

double ModelConfigPanel::balancingMaxRatio() const {
    return ui->ratioSpinBox->value();
}

bool ModelConfigPanel::hasValidConfiguration() const {
    return !selectedModelName().empty();
}

// =============================================================================
// Private slots
// =============================================================================

void ModelConfigPanel::_onTaskTypeChanged(int index) {
    Q_UNUSED(index);
    _populateAlgorithms();
    _updateBalancingVisibility();
}

void ModelConfigPanel::_onAlgorithmChanged(int index) {
    if (_updating) {
        return;
    }

    if (index < 0) {
        ui->parameterStack->setCurrentIndex(kPageEmpty);
        if (_state) {
            _state->setSelectedModelName({});
        }
        emit modelChanged({});
        return;
    }

    auto const name = ui->algorithmComboBox->currentData().toString().toStdString();
    _switchParameterPage(name);

    if (_state) {
        _state->setSelectedModelName(name);
    }

    emit modelChanged(QString::fromStdString(name));
}

void ModelConfigPanel::_onTrainClicked() {
    _syncToState();
    emit trainRequested();
}

void ModelConfigPanel::_onBalancingToggled(bool enabled) {
    ui->strategyLabel->setEnabled(enabled);
    ui->strategyComboBox->setEnabled(enabled);
    ui->ratioLabel->setEnabled(enabled);
    ui->ratioSpinBox->setEnabled(enabled);

    if (!_updating && _state) {
        _state->setBalancingEnabled(enabled);
    }

    emit balancingChanged();
}

void ModelConfigPanel::_onParameterValueChanged() {
    if (!_updating) {
        _syncToState();
        emit parametersChanged();
    }
}

// =============================================================================
// Setup helpers
// =============================================================================

void ModelConfigPanel::_setupConnections() {
    // Task type and algorithm combos
    connect(ui->taskTypeComboBox, &QComboBox::currentIndexChanged,
            this, &ModelConfigPanel::_onTaskTypeChanged);
    connect(ui->algorithmComboBox, &QComboBox::currentIndexChanged,
            this, &ModelConfigPanel::_onAlgorithmChanged);

    // Train button
    connect(ui->trainButton, &QPushButton::clicked,
            this, &ModelConfigPanel::_onTrainClicked);

    // Balancing
    connect(ui->enableBalancingCheckBox, &QCheckBox::toggled,
            this, &ModelConfigPanel::_onBalancingToggled);
    connect(ui->strategyComboBox, &QComboBox::currentIndexChanged,
            this, [this](int) {
                if (!_updating && _state) {
                    int const idx = ui->strategyComboBox->currentIndex();
                    _state->setBalancingStrategy(idx == 1 ? "oversample" : "subsample");
                }
                emit balancingChanged();
            });
    connect(ui->ratioSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double val) {
                if (!_updating && _state) {
                    _state->setBalancingMaxRatio(val);
                }
                emit balancingChanged();
            });

    // Random Forest parameter changes
    connect(ui->rfNumTreesSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ModelConfigPanel::_onParameterValueChanged);
    connect(ui->rfMinLeafSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ModelConfigPanel::_onParameterValueChanged);
    connect(ui->rfMaxDepthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ModelConfigPanel::_onParameterValueChanged);

    // Naive Bayes parameter changes
    connect(ui->nbEpsilonSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ModelConfigPanel::_onParameterValueChanged);

    // Logistic Regression parameter changes
    connect(ui->lrLambdaSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ModelConfigPanel::_onParameterValueChanged);
    connect(ui->lrMaxIterSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ModelConfigPanel::_onParameterValueChanged);

    // K-Means parameter changes
    connect(ui->kmKSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ModelConfigPanel::_onParameterValueChanged);
    connect(ui->kmMaxIterSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ModelConfigPanel::_onParameterValueChanged);
    connect(ui->kmSeedSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ModelConfigPanel::_onParameterValueChanged);

    // DBSCAN parameter changes
    connect(ui->dbEpsilonSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ModelConfigPanel::_onParameterValueChanged);
    connect(ui->dbMinPointsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ModelConfigPanel::_onParameterValueChanged);

    // GMM parameter changes
    connect(ui->gmmKSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ModelConfigPanel::_onParameterValueChanged);
    connect(ui->gmmMaxIterSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ModelConfigPanel::_onParameterValueChanged);
    connect(ui->gmmSeedSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ModelConfigPanel::_onParameterValueChanged);
}

void ModelConfigPanel::_populateTaskTypes() {
    ui->taskTypeComboBox->clear();

    ui->taskTypeComboBox->addItem(
            QStringLiteral("Binary Classification"),
            static_cast<int>(MLCore::MLTaskType::BinaryClassification));
    ui->taskTypeComboBox->addItem(
            QStringLiteral("Multi-Class Classification"),
            static_cast<int>(MLCore::MLTaskType::MultiClassClassification));
    ui->taskTypeComboBox->addItem(
            QStringLiteral("Clustering"),
            static_cast<int>(MLCore::MLTaskType::Clustering));

    // Default to Multi-Class Classification (covers binary too)
    ui->taskTypeComboBox->setCurrentIndex(1);
}

void ModelConfigPanel::_populateAlgorithms() {
    _updating = true;

    auto const task_variant = ui->taskTypeComboBox->currentData();
    if (!task_variant.isValid()) {
        _updating = false;
        return;
    }

    auto const task = static_cast<MLCore::MLTaskType>(task_variant.toInt());

    QString const previous_selection = ui->algorithmComboBox->currentData().toString();
    ui->algorithmComboBox->clear();

    auto const model_names = _registry->getModelNames(task);
    for (auto const & name: model_names) {
        ui->algorithmComboBox->addItem(
                QString::fromStdString(name),
                QString::fromStdString(name));
    }

    // Try to restore previous selection
    if (!previous_selection.isEmpty()) {
        int const idx = ui->algorithmComboBox->findData(previous_selection);
        if (idx >= 0) {
            ui->algorithmComboBox->setCurrentIndex(idx);
        }
    }

    _updating = false;

    // Trigger page switch for current selection
    if (ui->algorithmComboBox->count() > 0) {
        _onAlgorithmChanged(ui->algorithmComboBox->currentIndex());
    } else {
        ui->parameterStack->setCurrentIndex(kPageEmpty);
    }
}

void ModelConfigPanel::_populateBalancingStrategies() {
    ui->strategyComboBox->clear();
    ui->strategyComboBox->addItem(QStringLiteral("Subsample (down-sample majority)"));
    ui->strategyComboBox->addItem(QStringLiteral("Oversample (up-sample minority)"));
}

void ModelConfigPanel::_switchParameterPage(std::string const & model_name) {
    int const page = _pageIndexForModel(model_name);
    ui->parameterStack->setCurrentIndex(page);
}

void ModelConfigPanel::_updateBalancingVisibility() {
    auto const task_variant = ui->taskTypeComboBox->currentData();
    if (!task_variant.isValid()) {
        ui->balancingGroupBox->setVisible(false);
        return;
    }

    auto const task = static_cast<MLCore::MLTaskType>(task_variant.toInt());

    // Balancing is only relevant for classification tasks
    bool const is_classification =
            (task == MLCore::MLTaskType::BinaryClassification ||
             task == MLCore::MLTaskType::MultiClassClassification);

    ui->balancingGroupBox->setVisible(is_classification);
}

void ModelConfigPanel::_syncToState() {
    if (!_state) {
        return;
    }

    _state->setSelectedModelName(selectedModelName());
    _state->setBalancingEnabled(isBalancingEnabled());
    _state->setBalancingStrategy(
            balancingStrategy() == MLCore::BalancingStrategy::Oversample
                    ? "oversample"
                    : "subsample");
    _state->setBalancingMaxRatio(balancingMaxRatio());

    // Serialize current parameters to JSON for state persistence
    // (reserved for future implementation — the individual spin box values
    // are the source of truth for now)
}

void ModelConfigPanel::_restoreFromState() {
    if (!_state) {
        return;
    }

    _updating = true;

    // Restore model selection
    auto const & model_name = _state->selectedModelName();
    if (!model_name.empty()) {
        // Find the task type for this model
        auto const task_type = _registry->getTaskType(model_name);
        if (task_type) {
            // Set task type combo
            int const task_idx = ui->taskTypeComboBox->findData(
                    static_cast<int>(*task_type));
            if (task_idx >= 0) {
                ui->taskTypeComboBox->setCurrentIndex(task_idx);
            }

            // Need to re-populate algorithms for the new task type
            _updating = false;
            _populateAlgorithms();
            _updating = true;

            // Set algorithm combo
            int const algo_idx = ui->algorithmComboBox->findData(
                    QString::fromStdString(model_name));
            if (algo_idx >= 0) {
                ui->algorithmComboBox->setCurrentIndex(algo_idx);
                _switchParameterPage(model_name);
            }
        }
    }

    // Restore balancing settings
    ui->enableBalancingCheckBox->setChecked(_state->balancingEnabled());
    auto const & strategy = _state->balancingStrategy();
    if (strategy == "oversample") {
        ui->strategyComboBox->setCurrentIndex(1);
    } else {
        ui->strategyComboBox->setCurrentIndex(0);
    }
    ui->ratioSpinBox->setValue(_state->balancingMaxRatio());

    _updating = false;

    // Update balancing UI enabled state
    _onBalancingToggled(ui->enableBalancingCheckBox->isChecked());
    _updateBalancingVisibility();
}

int ModelConfigPanel::_pageIndexForModel(std::string const & name) const {
    if (name == "Random Forest") {
        return kPageRandomForest;
    }
    if (name == "Naive Bayes") {
        return kPageNaiveBayes;
    }
    if (name == "Logistic Regression") {
        return kPageLogisticRegression;
    }
    if (name == "K-Means") {
        return kPageKMeans;
    }
    if (name == "DBSCAN") {
        return kPageDBSCAN;
    }
    if (name == "Gaussian Mixture") {
        return kPageGMM;
    }
    return kPageEmpty;
}
