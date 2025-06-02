#include "ML_Widget.hpp"
#include "ui_ML_Widget.h"

#include "DataManager.hpp"

//https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots
#include "mlpack_conversion.hpp"
#include "DataManager/Tensors/Tensor_Data.hpp"
#define slots Q_SLOTS

#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/Points/Point_Data.hpp"

#include "DataManager/utils/armadillo_wrap/analog_armadillo.hpp"
#include "MLModelOperation.hpp"
#include "MLModelRegistry.hpp"
#include "MLParameterWidgetBase.hpp"
#include "ML_Naive_Bayes_Widget/ML_Naive_Bayes_Widget.hpp"
#include "ML_Random_Forest_Widget/ML_Random_Forest_Widget.hpp"
#include "TimeFrame.hpp"
#include "Transformations/IdentityTransform.hpp"
#include "Transformations/LagLeadTransform.hpp"
#include "Transformations/SquaredTransform.hpp"

#include "mlpack.hpp"
#include <QFileDialog>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>

#include <fstream>
#include <iostream>
#include <numeric>
#include <optional>


ML_Widget::ML_Widget(std::shared_ptr<DataManager> data_manager,
                     QWidget * parent)
    : QWidget(parent),
      _data_manager{std::move(data_manager)},
      _ml_model_registry(std::make_unique<MLModelRegistry>()),
      ui(new Ui::ML_Widget) {
    ui->setupUi(this);

    // Initialize Transformation Registry
    _transformation_registry[WhiskerTransformations::TransformationType::Identity] = std::make_unique<IdentityTransform>();
    _transformation_registry[WhiskerTransformations::TransformationType::Squared] = std::make_unique<SquaredTransform>();
    _transformation_registry[WhiskerTransformations::TransformationType::LagLead] = std::make_unique<LagLeadTransform>();

    auto naive_bayes_widget = new ML_Naive_Bayes_Widget(_data_manager);
    int nb_idx = ui->stackedWidget->addWidget(naive_bayes_widget);
    _model_name_to_widget_index["Naive Bayes"] = nb_idx;

    auto random_forest_widget = new ML_Random_Forest_Widget(_data_manager);
    int rf_idx = ui->stackedWidget->addWidget(random_forest_widget);
    _model_name_to_widget_index["Random Forest"] = rf_idx;

    ui->model_select_combo->clear();
    std::vector<std::string> model_names = _ml_model_registry->getAvailableModelNames();
    for (auto const & name: model_names) {
        ui->model_select_combo->addItem(QString::fromStdString(name));
    }
    if (!model_names.empty()) {
        _selectModelType(QString::fromStdString(model_names[0]));
    } else {
        ui->fit_button->setEnabled(false);
    }

    // Feature Processing Widget Setup
    _feature_processing_widget = ui->feature_processing_widget;// Promoted from UI
    _feature_processing_widget->setDataManager(_data_manager.get());
    _feature_processing_widget->populateBaseFeatures();
    connect(_feature_processing_widget, &FeatureProcessingWidget::configurationChanged,
            this, &ML_Widget::_updateClassDistribution);

    // Training Interval ComboBox Setup (already done in previous steps)
    connect(ui->trainingIntervalComboBox, &QComboBox::currentTextChanged, this, &ML_Widget::_onTrainingIntervalChanged);

    //Outcome Table Widget
    ui->outcome_table_widget->setColumns({"Feature", "Enabled", "Type"});
    ui->outcome_table_widget->setTypeFilter({DM_DataType::Analog, DM_DataType::DigitalInterval, DM_DataType::Points, DM_DataType::Tensor});
    ui->outcome_table_widget->setDataManager(_data_manager);
    connect(ui->outcome_table_widget, &Feature_Table_Widget::featureSelected, this, &ML_Widget::_handleOutcomeSelected);
    connect(ui->outcome_table_widget, &Feature_Table_Widget::addFeature, this, [this](QString const & feature) {
        ML_Widget::_addOutcomeToModel(feature, true);
    });
    connect(ui->outcome_table_widget, &Feature_Table_Widget::removeFeature, this, [this](QString const & feature) {
        ML_Widget::_addOutcomeToModel(feature, false);
    });

    connect(ui->model_select_combo, &QComboBox::currentTextChanged, this, &ML_Widget::_selectModelType);
    connect(ui->fit_button, &QPushButton::clicked, this, &ML_Widget::_fitModel);

    _class_balancing_widget = ui->class_balancing_widget;
    connect(_class_balancing_widget, &ClassBalancingWidget::balancingSettingsChanged, this, &ML_Widget::_updateClassDistribution);

    _model_metrics_widget = ui->model_metrics_widget;

    _data_manager->addObserver([this]() {
        _populateTrainingIntervalComboBox();
        if (_feature_processing_widget) {// Repopulate features if DM changes
            _feature_processing_widget->populateBaseFeatures();
        }
    });
    _populateTrainingIntervalComboBox();
}

ML_Widget::~ML_Widget() {
    delete ui;
}

void ML_Widget::openWidget() {
    std::cout << "ML Widget Opened" << std::endl;
    if (_feature_processing_widget) {
        _feature_processing_widget->populateBaseFeatures();
    }
    ui->outcome_table_widget->populateTable();
    _populateTrainingIntervalComboBox();// Ensure this is up-to-date too

    this->show();
}

void ML_Widget::closeEvent(QCloseEvent * event) {
    std::cout << "Close event detected" << std::endl;
    QWidget::closeEvent(event);
}

void ML_Widget::_onTrainingIntervalChanged(QString const & intervalKey) {
    _training_interval_key = intervalKey;
    _updateClassDistribution();
}

void ML_Widget::_populateTrainingIntervalComboBox() {
    QString currentSelection = ui->trainingIntervalComboBox->currentText();
    ui->trainingIntervalComboBox->blockSignals(true);
    ui->trainingIntervalComboBox->clear();

    std::vector<std::string> intervalKeys = _data_manager->getKeys<DigitalIntervalSeries>();
    ui->trainingIntervalComboBox->addItem("");
    for (auto const & key: intervalKeys) {
        ui->trainingIntervalComboBox->addItem(QString::fromStdString(key));
    }

    if (intervalKeys.empty()) {
        _training_interval_key.clear();
    }

    int index = ui->trainingIntervalComboBox->findText(currentSelection);
    if (index != -1) {
        ui->trainingIntervalComboBox->setCurrentIndex(index);
        _training_interval_key = currentSelection;
    } else if (!intervalKeys.empty() && ui->trainingIntervalComboBox->count() > 1) {
        bool currentKeyIsValid = false;
        for (auto const & key: intervalKeys) {
            if (QString::fromStdString(key) == _training_interval_key) {
                currentKeyIsValid = true;
                int validIdx = ui->trainingIntervalComboBox->findText(_training_interval_key);
                if (validIdx != -1) ui->trainingIntervalComboBox->setCurrentIndex(validIdx);
                else
                    ui->trainingIntervalComboBox->setCurrentIndex(0);
                break;
            }
        }
        if (!currentKeyIsValid) {
            ui->trainingIntervalComboBox->setCurrentIndex(1);
            _training_interval_key = ui->trainingIntervalComboBox->currentText();
        }
    } else {
        _training_interval_key.clear();
        if (ui->trainingIntervalComboBox->count() > 0) ui->trainingIntervalComboBox->setCurrentIndex(0);
    }

    ui->trainingIntervalComboBox->blockSignals(false);
    _updateClassDistribution();
}

void ML_Widget::_handleOutcomeSelected(QString const & feature) {
    // For now, this might not do much if selection is handled by the table widget itself
    // and _selected_outcomes is populated by add/remove signals.
    // Placeholder if specific action on selection is needed later.
}

void ML_Widget::_addOutcomeToModel(QString const & feature, bool enabled) {
    if (enabled) {
        _selected_outcomes.insert(feature.toStdString());
    } else {
        _selected_outcomes.erase(feature.toStdString());
    }
    _updateClassDistribution();// Outcomes also affect distribution text
}

void ML_Widget::_removeSelectedOutcome(std::string const & key) {
    _selected_outcomes.erase(key);
    _updateClassDistribution();
}

void ML_Widget::_selectModelType(QString const & model_type_qstr) {
    std::string model_type = model_type_qstr.toStdString();
    _current_selected_model_operation = _ml_model_registry->findOperationByName(model_type);

    if (_current_selected_model_operation) {
        auto it = _model_name_to_widget_index.find(model_type);
        if (it != _model_name_to_widget_index.end()) {
            ui->stackedWidget->setCurrentIndex(it->second);
            ui->fit_button->setEnabled(true);
        } else {
            std::cerr << "Model UI widget not found for: " << model_type << std::endl;
            ui->stackedWidget->setCurrentIndex(0);// Default to first or handle error
            ui->fit_button->setEnabled(false);
        }
    } else {
        std::cerr << "Unsupported Model Type Selected: " << model_type << std::endl;
        ui->stackedWidget->setCurrentIndex(0);// Default to first or handle error
        ui->fit_button->setEnabled(false);
    }
}

void ML_Widget::_fitModel() {
    if (!_current_selected_model_operation) {
        std::cerr << "No model operation selected." << std::endl;
        return;
    }

    // Clear previous metrics at start of new fit
    _model_metrics_widget->clearMetrics();

    std::vector<FeatureProcessingWidget::ProcessedFeatureInfo> active_proc_features =
            _feature_processing_widget->getActiveProcessedFeatures();

    if (active_proc_features.empty() || _training_interval_key.isEmpty() || _selected_outcomes.empty()) {
        std::cerr << "Please select features (and configure transformations), a training data interval, and outcomes" << std::endl;
        return;
    }

    // Prepare training data
    std::vector<std::size_t> training_timestamps;
    arma::Mat<double> feature_array;
    arma::Mat<double> outcome_array;
    
    auto labels_opt = _prepareTrainingData(active_proc_features, training_timestamps, feature_array, outcome_array);
    if (!labels_opt.has_value()) {
        std::cerr << "Failed to prepare training data. Aborting fit." << std::endl;
        return;
    }
    
    arma::Row<size_t> labels = labels_opt.value();

    // Train the model
    arma::Mat<double> balanced_feature_array;
    arma::Row<size_t> balanced_labels;
    
    if (!_trainModel(feature_array, labels, balanced_feature_array, balanced_labels)) {
        std::cerr << "Model training failed." << std::endl;
        return;
    }

    // Predict on new data
    if (!_predictNewData(active_proc_features, training_timestamps)) {
        std::cerr << "Prediction on new data failed." << std::endl;
    }

    std::cout << "Exiting _fitModel" << std::endl;
}

std::optional<arma::Row<size_t>> ML_Widget::_prepareTrainingData(
        std::vector<FeatureProcessingWidget::ProcessedFeatureInfo> const & active_proc_features,
        std::vector<std::size_t> & training_timestamps,
        arma::Mat<double> & feature_array,
        arma::Mat<double> & outcome_array) const {
    
    // Get training interval data and create timestamps
    auto training_interval_series = _data_manager->getData<DigitalIntervalSeries>(_training_interval_key.toStdString());
    if (!training_interval_series) {
        std::cerr << "Could not retrieve training interval data: " << _training_interval_key.toStdString() << std::endl;
        return std::nullopt;
    }

    training_timestamps = create_timestamps(training_interval_series);
    if (training_timestamps.empty()) {
        std::cerr << "No training timestamps generated from the selected interval: " << _training_interval_key.toStdString() << std::endl;
        return std::nullopt;
    }
    
    std::sort(training_timestamps.begin(), training_timestamps.end());
    training_timestamps.erase(std::unique(training_timestamps.begin(), training_timestamps.end()), training_timestamps.end());
    std::cout << "Number of unique training timestamps: " << training_timestamps.size() << std::endl;

    // Create feature matrix
    std::string feature_matrix_error;
    feature_array = _createFeatureMatrix(active_proc_features, training_timestamps, feature_matrix_error);
    if (!feature_matrix_error.empty()) {
        std::cerr << "Error(s) creating feature matrix:\n" << feature_matrix_error << std::endl;
    }
    if (feature_array.n_cols == 0) {
        std::cerr << "Feature array for training is empty or could not be created." << std::endl;
        return std::nullopt;
    }
    std::cout << "Training feature array size: " << feature_array.n_rows << " x " << feature_array.n_cols << std::endl;

    // Remove NaN columns from training data
    feature_array = _removeNaNColumns(feature_array, training_timestamps);
    if (feature_array.n_cols == 0) {
        std::cerr << "No valid training data remains after NaN removal." << std::endl;
        return std::nullopt;
    }
    std::cout << "Training feature array size after NaN removal: " << feature_array.n_rows << " x " << feature_array.n_cols << std::endl;

    // Apply z-score normalization if enabled
    if (_feature_processing_widget->isZScoreNormalizationEnabled()) {
        feature_array = _zScoreNormalizeFeatures(feature_array, active_proc_features);
        std::cout << "Applied z-score normalization to training features" << std::endl;
    }

    // Create outcome array
    outcome_array = create_arrays(_selected_outcomes, training_timestamps, _data_manager.get());
    if (outcome_array.n_cols == 0 && !training_timestamps.empty()) {
        std::cerr << "Outcome array for training is empty (0 columns), though timestamps were present." << std::endl;
        return std::nullopt;
    }
    std::cout << "Training outcome array size: " << outcome_array.n_rows << " x " << outcome_array.n_cols << std::endl;

    // Create labels from outcome array
    arma::Row<size_t> labels;
    if (outcome_array.n_rows > 0) {
        labels = arma::conv_to<arma::Row<size_t>>::from(outcome_array.row(0));
    } else if (!training_timestamps.empty()) {
        std::cerr << "Outcome array has 0 rows, but training timestamps exist. Cannot create labels." << std::endl;
        return std::nullopt;
    }

    if (labels.empty() && !training_timestamps.empty()) {
        std::cerr << "Labels are empty, cannot proceed with model training." << std::endl;
        return std::nullopt;
    }

    return labels;
}

bool ML_Widget::_trainModel(arma::Mat<double> const & feature_array,
                            arma::Row<size_t> const & labels,
                            arma::Mat<double> & balanced_feature_array,
                            arma::Row<size_t> & balanced_labels) {
    
    auto const balancing_flag = _class_balancing_widget->isBalancingEnabled();
    std::cout << "Balancing is set to " << balancing_flag << std::endl;

    // Handle class balancing
    if (balancing_flag && !labels.empty()) {
        double ratio = _class_balancing_widget->getBalancingRatio();
        if (!balance_training_data_by_subsampling(feature_array, labels, balanced_feature_array, balanced_labels, ratio)) {
            std::cerr << "Data balancing failed. Proceeding with original data, but results may be skewed." << std::endl;
            balanced_feature_array = feature_array;
            balanced_labels = labels;
        }
    } else {
        balanced_feature_array = feature_array;
        balanced_labels = labels;
        if (labels.empty()) {
            std::cout << "Class balancing skipped as labels are empty." << std::endl;
        } else {
            std::cout << "Class balancing disabled or skipped - using original data distribution." << std::endl;
        }
    }

    if ((balanced_feature_array.n_cols == 0 || balanced_labels.n_elem == 0) && !labels.empty()) {
        std::cerr << "No data remains after potential balancing. Cannot train model." << std::endl;
        return false;
    }

    // Get model parameters
    std::unique_ptr<MLModelParametersBase> model_params_ptr;
    MLParameterWidgetBase * current_param_widget = dynamic_cast<MLParameterWidgetBase *>(ui->stackedWidget->currentWidget());
    if (current_param_widget) {
        model_params_ptr = current_param_widget->getParameters();
    } else {
        std::cerr << "Could not get parameter widget for selected model." << std::endl;
        return false;
    }

    if (!model_params_ptr) {
        std::cerr << "Failed to retrieve model parameters from UI." << std::endl;
        return false;
    }

    // Train the model
    bool trained = _current_selected_model_operation->train(balanced_feature_array, balanced_labels, model_params_ptr.get());
    if (!trained) {
        std::cerr << "Model training failed for " << _current_selected_model_operation->getName() << std::endl;
        return false;
    }
    std::cout << "Model trained: " << _current_selected_model_operation->getName() << std::endl;

    // Calculate training accuracy and detailed metrics
    arma::Row<size_t> training_predictions;
    if (balanced_feature_array.n_cols > 0) {
        bool training_predicted = _current_selected_model_operation->predict(balanced_feature_array, training_predictions);
        if (training_predicted && balanced_labels.n_elem > 0) {
            // Calculate basic accuracy (for console output)
            double const accuracy = 100.0 * (static_cast<double>(arma::accu(training_predictions == balanced_labels))) /
                                    static_cast<double>(balanced_labels.n_elem);
            std::cout << "Training set accuracy (on potentially balanced data) is " << accuracy << "%." << std::endl;
            
            // Update metrics widget with detailed binary classification metrics
            std::string model_name = _current_selected_model_operation->getName();
            _model_metrics_widget->setBinaryClassificationMetrics(training_predictions, balanced_labels, model_name);
        } else if (balanced_labels.n_elem > 0) {
            std::cerr << "Model prediction on training data failed." << std::endl;
            _model_metrics_widget->clearMetrics();
        }
    }

    return true;
}

bool ML_Widget::_predictNewData(std::vector<FeatureProcessingWidget::ProcessedFeatureInfo> const & active_proc_features,
                                std::vector<std::size_t> const & training_timestamps) {
    
    // Determine prediction timestamps
    int current_time_end_frame;
    if (ui->predict_all_check->isChecked()) {
        current_time_end_frame = _data_manager->getTime()->getTotalFrameCount();
    } else {
        std::cout << "Prediction not set to predict all frames." << std::endl;
        return true;
    }
    if (_data_manager->getTime()->getTotalFrameCount() == 0) current_time_end_frame = 0;

    std::vector<std::size_t> prediction_timestamps;
    if (current_time_end_frame > 0) {
        std::unordered_set<std::size_t> train_ts_set(training_timestamps.begin(), training_timestamps.end());
        for (std::size_t i = 0; i < static_cast<std::size_t>(current_time_end_frame); ++i) {
            if (train_ts_set.find(i) == train_ts_set.end()) {
                prediction_timestamps.push_back(i);
            }
        }
    }

    if (prediction_timestamps.empty()) {
        std::cout << "No frames identified for prediction." << std::endl;
        return true; // Not an error condition
    }

    std::cout << "Number of prediction timestamps: " << prediction_timestamps.size()
              << " (Range: " << prediction_timestamps.front() << " to " << prediction_timestamps.back() << ")" << std::endl;

    // Create feature matrix for prediction
    std::string pred_feature_matrix_error;
    arma::Mat<double> prediction_feature_array = _createFeatureMatrix(
            active_proc_features, prediction_timestamps, pred_feature_matrix_error);

    if (!pred_feature_matrix_error.empty()) {
        std::cerr << "Error(s) creating prediction feature matrix:\n" << pred_feature_matrix_error << std::endl;
    }

    std::cout << "The prediction mask nan values: " << prediction_feature_array.has_nan() << std::endl;
    prediction_feature_array.replace(arma::datum::nan, 0.0); // Prediction fails if there is a NaN value
    std::cout << "After nan replacement: " << prediction_feature_array.has_nan() << std::endl;

    // Apply z-score normalization if enabled
    if (_feature_processing_widget->isZScoreNormalizationEnabled()) {
        prediction_feature_array = _zScoreNormalizeFeatures(prediction_feature_array, active_proc_features);
    }

    if (prediction_feature_array.n_cols == 0) {
        std::cout << "No features to predict for the selected prediction timestamps (prediction_feature_array is empty)." << std::endl;
        return true; // Not an error condition
    }

    // Make predictions
    arma::Row<size_t> future_predictions;
    bool future_predicted = _current_selected_model_operation->predict(prediction_feature_array, future_predictions);

    if (!future_predicted) {
        std::cerr << "Prediction on new data failed." << std::endl;
        return false;
    }

    // Apply predictions to outcome series
    auto prediction_vec = copyMatrixRowToVector<size_t>(future_predictions);
    if (!prediction_vec.empty()) {
        std::cout << "Range of predictions on new data. Max: " << *std::max_element(prediction_vec.begin(), prediction_vec.end())
                  << ", Min: " << *std::min_element(prediction_vec.begin(), prediction_vec.end()) << std::endl;
    } else {
        std::cout << "Prediction vector on new data is empty." << std::endl;
    }

    for (auto const & key: _selected_outcomes) {
        auto outcome_series = _data_manager->getData<DigitalIntervalSeries>(key);
        if (outcome_series) {
            outcome_series->setEventsAtTimes(prediction_timestamps, prediction_vec);
            std::cout << "Predictions applied to outcome series: " << key << std::endl;
        } else {
            std::cerr << "Could not get outcome series '" << key << "' to apply predictions." << std::endl;
        }
    }

    return true;
}

void ML_Widget::_updateClassDistribution() {
    bool features_selected = (_feature_processing_widget &&
                              !_feature_processing_widget->getActiveProcessedFeatures().empty());

    if (!features_selected || _training_interval_key.isEmpty() || _selected_outcomes.empty()) {
        _class_balancing_widget->clearClassDistribution();
        return;
    }

    try {
        auto current_mask_series = _data_manager->getData<DigitalIntervalSeries>(_training_interval_key.toStdString());
        if (!current_mask_series) {
            _class_balancing_widget->clearClassDistribution();
            std::cerr << "Could not retrieve training interval data for distribution: " << _training_interval_key.toStdString() << std::endl;
            return;
        }
        std::vector<std::size_t> timestamps = create_timestamps(current_mask_series);
        if (timestamps.empty()) {
            _class_balancing_widget->clearClassDistribution();
            std::cout << "No timestamps generated from training interval for distribution update." << std::endl;
            return;
        }

        // For distribution, we only need outcomes
        arma::Mat<double> outcome_array_dist = create_arrays(_selected_outcomes, timestamps, _data_manager.get());
        if (outcome_array_dist.empty() && !timestamps.empty()) {
            _class_balancing_widget->clearClassDistribution();
            std::cerr << "Outcome array for distribution is empty." << std::endl;
            return;
        }
        if (outcome_array_dist.n_rows == 0 && !timestamps.empty()) {
            _class_balancing_widget->clearClassDistribution();
            std::cerr << "Outcome array for distribution has 0 rows." << std::endl;
            return;
        }

        arma::Row<size_t> labels_dist;
        if (!outcome_array_dist.empty() && outcome_array_dist.n_rows > 0) {
            labels_dist = arma::conv_to<arma::Row<size_t>>::from(outcome_array_dist.row(0));
        } else if (timestamps.empty()) {
            _class_balancing_widget->clearClassDistribution();// No data to show
            return;
        } else {
            _class_balancing_widget->clearClassDistribution();
            std::cerr << "Cannot form labels for distribution display." << std::endl;
            return;
        }


        std::map<size_t, size_t> class_counts;
        for (size_t i = 0; i < labels_dist.n_elem; ++i) {
            class_counts[labels_dist[i]]++;
        }

        if (class_counts.empty() && !labels_dist.empty()) {
            // This case (labels exist but no counts) should ideally not happen if labels_dist has elements
            //_class_balancing_widget->setText("Original: No class counts found despite having labels.");
            return;
        } else if (class_counts.empty() && labels_dist.empty() && !timestamps.empty()) {
            //_class_balancing_widget->setText("Original: No labels found for selected outcomes and timestamps.");
            return;
        } else if (class_counts.empty()) {
            _class_balancing_widget->clearClassDistribution();
            return;
        }


        QString distributionText = "Original: ";
        bool first = true;
        for (auto const & [label_val, count]: class_counts) {
            if (!first) distributionText += ", ";
            distributionText += QString("Class %1: %L2 samples").arg(label_val).arg(count);
            first = false;
        }

        if (_class_balancing_widget->isBalancingEnabled()) {
            size_t min_class_count = std::numeric_limits<size_t>::max();
            bool found_any_class_with_samples = false;
            for (auto const & [label_val, count]: class_counts) {
                if (count > 0) {
                    found_any_class_with_samples = true;
                    if (count < min_class_count) {
                        min_class_count = count;
                    }
                }
            }
            if (!found_any_class_with_samples) min_class_count = 0;

            double ratio = _class_balancing_widget->getBalancingRatio();
            size_t target_max_samples = (min_class_count > 0) ? static_cast<size_t>(std::round(static_cast<double>(min_class_count) * ratio)) : 0;
            if (target_max_samples == 0 && min_class_count > 0 && ratio >= 1.0) target_max_samples = 1;

            distributionText += "\nBalanced (estimated): ";
            first = true;
            for (auto const & [label_val, count]: class_counts) {
                if (!first) distributionText += ", ";
                size_t balanced_count = 0;
                if (count > 0) {
                    balanced_count = std::min(count, target_max_samples);
                    if (balanced_count == 0 && target_max_samples > 0 && count > 0) balanced_count = 1;
                }
                distributionText += QString("Class %1: %L2 samples").arg(label_val).arg(balanced_count);
                first = false;
            }
        }
        _class_balancing_widget->updateClassDistribution(distributionText);

    } catch (std::exception const & e) {
        _class_balancing_widget->clearClassDistribution();
        std::cerr << "Error updating class distribution: " << e.what() << std::endl;
    }
}

arma::Mat<double> ML_Widget::_createFeatureMatrix(
        std::vector<FeatureProcessingWidget::ProcessedFeatureInfo> const & processed_features,
        std::vector<std::size_t> const & timestamps,
        std::string & error_message) const {
    arma::Mat<double> final_feature_matrix;
    if (processed_features.empty()) {
        error_message = "No features selected or processed.";
        return final_feature_matrix;// Return empty matrix
    }
    if (timestamps.empty()) {
        error_message = "No timestamps provided for feature matrix creation.";
        return final_feature_matrix;
    }

    std::vector<arma::Mat<double>> feature_component_matrices;

    for (auto const & p_feature: processed_features) {
        // arma::Mat<double> current_feature_data_matrix; // Will be set by the transformation strategy
        auto base_key = p_feature.base_feature_key;

        DM_DataType data_type = _data_manager->getType(base_key);

        auto it = _transformation_registry.find(p_feature.transformation.type);
        if (it == _transformation_registry.end()) {
            error_message += "Unsupported transformation type '" +
                             QString::number(static_cast<int>(p_feature.transformation.type)).toStdString() +// Basic way to show type
                             "' for feature '" + base_key + "'. No registered strategy found.\n";
            continue;
        }

        auto const & transform_strategy = it->second;
        arma::Mat<double> current_feature_data_matrix = transform_strategy->apply(
                _data_manager.get(), base_key, data_type, timestamps,
                p_feature.transformation,
                error_message);

        std::cout << "Transformation resulted in feature data matrix of size "
                  << current_feature_data_matrix.n_cols << " x "
                  << current_feature_data_matrix.n_rows << std::endl;

        if (!error_message.empty() && current_feature_data_matrix.empty()) {
            // Error message already populated by the apply method or internal checks
            // No need to add "Warning: Data for feature..." again if apply itself reported it.
            // Just ensure error_message is cumulative if needed.
            // std::cout << "Error from transform apply: " << error_message << std::endl; // For debugging
            continue;// Skip this feature if apply failed and returned empty matrix with error
        }
        if (current_feature_data_matrix.empty() && error_message.empty()) {
            // This case means apply returned empty but didn't set an error, which might be an issue
            // in the transform's implementation or a valid case of no data for this specific transform.
            error_message += "Warning: Transformation for feature '" + base_key + "' resulted in an empty matrix without explicit error. Skipping.\n";
            continue;
        }

        // The old data fetching and transformation specific if/else block is now replaced by the strategy call.

        feature_component_matrices.push_back(current_feature_data_matrix);
    }

    if (feature_component_matrices.empty()) {
        error_message = "No feature components were successfully processed into matrices.";
        return final_feature_matrix;// Return empty
    }

    // Concatenate all feature component matrices. They should all have timestamps.size() columns.
    // Armadillo join_rows (vertical concatenation) is what we need if each matrix has features as rows and samples as columns.
    // The conversion functions (e.g. convertAnalogTimeSeriesToMlpackArray) return arma::Row<double> (1 row, N samples).
    // So we need to transpose them to be N rows, 1 sample when stacking, then transpose back, OR ensure all are (num_features_for_this_component x num_samples).
    // Let's standardize that each current_feature_data_matrix is (num_sub_features x num_timestamps).
    // convertAnalogTimeSeriesToMlpackArray returns (1 x num_timestamps), so it's fine.
    // convertToMlpackArray (DigitalInterval) returns (1 x num_timestamps), fine.
    // convertToMlpackMatrix (Points) returns (num_point_coords x num_timestamps), fine.
    // convertTensorDataToMlpackMatrix returns (flattened_tensor_dim x num_timestamps), fine.

    final_feature_matrix = feature_component_matrices[0];
    for (size_t i = 1; i < feature_component_matrices.size(); ++i) {
        if (feature_component_matrices[i].n_cols == final_feature_matrix.n_cols) {
            final_feature_matrix = arma::join_cols(final_feature_matrix, feature_component_matrices[i]);
        } else {
            error_message += "Error: Mismatched number of samples (columns) when joining feature matrices. Skipping a component.";
            // Potentially skip this component or handle error more gracefully
        }
    }
    return final_feature_matrix;
}

arma::Mat<double> ML_Widget::_removeNaNColumns(arma::Mat<double> const & matrix, std::vector<std::size_t> & timestamps) const {
    if (matrix.empty() || timestamps.empty()) {
        return matrix;
    }
    
    std::vector<arma::uword> valid_columns;
    
    // Check each column for NaN values
    for (arma::uword col = 0; col < matrix.n_cols; ++col) {
        bool has_nan = false;
        for (arma::uword row = 0; row < matrix.n_rows; ++row) {
            if (!std::isfinite(matrix(row, col))) {
                has_nan = true;
                break;
            }
        }
        if (!has_nan) {
            valid_columns.push_back(col);
        }
    }
    
    size_t original_cols = matrix.n_cols;
    size_t removed_cols = original_cols - valid_columns.size();
    
    if (removed_cols > 0) {
        std::cout << "Removed " << removed_cols << " timestamp columns containing NaN values out of " 
                  << original_cols << " total columns (" 
                  << (100.0 * removed_cols / original_cols) << "% removed)" << std::endl;
    }
    
    if (valid_columns.empty()) {
        std::cout << "Warning: All columns contained NaN values. Returning empty matrix." << std::endl;
        timestamps.clear();
        return arma::Mat<double>();
    }
    
    // Create new matrix with only valid columns
    arma::Mat<double> cleaned_matrix(matrix.n_rows, valid_columns.size());
    std::vector<std::size_t> cleaned_timestamps;
    
    for (size_t i = 0; i < valid_columns.size(); ++i) {
        cleaned_matrix.col(i) = matrix.col(valid_columns[i]);
        cleaned_timestamps.push_back(timestamps[valid_columns[i]]);
    }
    
    timestamps = cleaned_timestamps;
    return cleaned_matrix;
}

arma::Mat<double> ML_Widget::_zScoreNormalizeFeatures(arma::Mat<double> const & matrix, 
        std::vector<FeatureProcessingWidget::ProcessedFeatureInfo> const & processed_features) const {
    if (matrix.empty()) {
        return matrix;
    }
    
    arma::Mat<double> normalized_matrix = matrix;
    arma::uword current_row = 0;
    
    for (auto const & p_feature : processed_features) {
        std::string base_key = p_feature.base_feature_key;
        DM_DataType data_type = _data_manager->getType(base_key);
        
        // Skip normalization for DigitalInterval features (binary)
        bool skip_normalization = (data_type == DM_DataType::DigitalInterval);
        
        // Determine how many rows this feature contributes
        arma::uword feature_rows = 1; // Default for most features
        
        if (data_type == DM_DataType::Points) {
            auto point_data = _data_manager->getData<PointData>(base_key);
            if (point_data) {
                feature_rows = point_data->getMaxPoints() * 2; // x,y coordinates
            }
        } else if (data_type == DM_DataType::Tensor) {
            auto tensor_data = _data_manager->getData<TensorData>(base_key);
            if (tensor_data) {
                auto feature_shape = tensor_data->getFeatureShape();
                feature_rows = std::accumulate(feature_shape.begin(), feature_shape.end(), 1, std::multiplies<>());
            }
        }
        
        // Apply lag/lead multiplier if applicable
        if (p_feature.transformation.type == WhiskerTransformations::TransformationType::LagLead) {
            if (auto* ll_params = std::get_if<WhiskerTransformations::LagLeadParams>(&p_feature.transformation.params)) {
                int num_shifts = ll_params->max_lead_steps - ll_params->min_lag_steps + 1;
                feature_rows *= num_shifts;
            }
        }
        
        if (!skip_normalization) {
            // Normalize each row (feature) separately
            for (arma::uword row = current_row; row < current_row + feature_rows; ++row) {
                if (row < normalized_matrix.n_rows) {
                    arma::rowvec feature_row = normalized_matrix.row(row);
                    
                    // Check if this row has any finite values
                    arma::uvec finite_indices = arma::find_finite(feature_row);
                    if (finite_indices.n_elem > 1) { // Need at least 2 values for std calculation
                        double mean_val = arma::mean(feature_row(finite_indices));
                        double std_val = arma::stddev(feature_row(finite_indices));
                        
                        if (std_val > 1e-10) { // Avoid division by zero
                            normalized_matrix.row(row) = (feature_row - mean_val) / std_val;
                        }
                    }
                }
            }
        }
        
        current_row += feature_rows;
    }
    
    return normalized_matrix;
}

std::vector<std::size_t> create_timestamps(std::vector<Interval> & intervals) {
    std::vector<std::size_t> timestamps;
    for (auto interval: intervals) {
        //I want to generate timestamps for each value between interval.start and interval.end
        for (std::size_t i = interval.start; i < interval.end; i++) {
            timestamps.push_back(i);
        }
    }

    return timestamps;
}

std::vector<std::size_t> create_timestamps(std::shared_ptr<DigitalIntervalSeries> & series) {
    auto intervals = series->getDigitalIntervalSeries();
    return create_timestamps(intervals);
}

/**
 * @brief Convert data keys to mlpack arrays for training/prediction
 * @param data_keys Set of data keys to convert
 * @param timestamps Timestamps to extract data for
 * @param data_manager DataManager instance to get data from
 * @return arma::Mat<double> Concatenated matrix with features as rows and samples as columns
 */
arma::Mat<double> create_arrays(
        std::unordered_set<std::string> const & data_keys,
        std::vector<std::size_t> & timestamps,
        DataManager * data_manager) {
    std::vector<arma::Mat<double>> component_arrays;

    for (auto const & key: data_keys) {
        auto const data_type = data_manager->getType(key);
        arma::Mat<double> current_array_component;

        if (data_type == DM_DataType::Analog) {
            auto analog_series = data_manager->getData<AnalogTimeSeries>(key);
            if (analog_series) current_array_component = convertAnalogTimeSeriesToMlpackArray(analog_series.get(), timestamps).t();
        } else if (data_type == DM_DataType::DigitalInterval) {
            auto digital_series = data_manager->getData<DigitalIntervalSeries>(key);
            if (digital_series) current_array_component = convertToMlpackArray(digital_series, timestamps).t();
        } else if (data_type == DM_DataType::Points) {
            auto point_data = data_manager->getData<PointData>(key);
            if (point_data) current_array_component = convertToMlpackMatrix(point_data, timestamps);
        } else if (data_type == DM_DataType::Tensor) {
            auto tensor_data = data_manager->getData<TensorData>(key);
            if (tensor_data) current_array_component = convertTensorDataToMlpackMatrix(*tensor_data, timestamps);
        } else {
            std::cerr << "Unsupported data type for key '" << key << "': " << convert_data_type_to_string(data_type) << std::endl;
            continue;
        }
        if (!current_array_component.empty()) {
            if (data_type == DM_DataType::Analog || data_type == DM_DataType::DigitalInterval) {
                component_arrays.push_back(current_array_component.t());
            } else {
                component_arrays.push_back(current_array_component);
            }
        }
    }

    arma::Mat<double> concatenated_array;
    if (!component_arrays.empty()) {
        concatenated_array = component_arrays[0];
        for (std::size_t i = 1; i < component_arrays.size(); i++) {
            if (component_arrays[i].n_cols == concatenated_array.n_cols) {
                concatenated_array = arma::join_cols(concatenated_array, component_arrays[i]);
            } else if (component_arrays[i].n_rows == concatenated_array.n_rows) {
                // This case should not happen if timestamps is common for all
            }
        }
    }
    return concatenated_array;
}
