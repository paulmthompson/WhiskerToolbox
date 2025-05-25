#include "ML_Widget.hpp"
#include "ui_ML_Widget.h"

#include "DataManager.hpp"

//https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots
#include "mlpack_conversion.hpp"
#define slots Q_SLOTS

#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/utils/armadillo_wrap/analog_armadillo.hpp"
#include "MLModelOperation.hpp"
#include "MLModelRegistry.hpp"
#include "MLParameterWidgetBase.hpp"
#include "ML_Naive_Bayes_Widget/ML_Naive_Bayes_Widget.hpp"
#include "ML_Random_Forest_Widget/ML_Random_Forest_Widget.hpp"
#include "TimeFrame.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"
#include "Transformations/IdentityTransform.hpp"
#include "Transformations/LagLeadTransform.hpp"
#include "Transformations/SquaredTransform.hpp"

#include "mlpack.hpp"
#include <QFileDialog>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>

#include <fstream>
#include <iostream>


ML_Widget::ML_Widget(std::shared_ptr<DataManager> data_manager,
                     TimeScrollBar * time_scrollbar,
                     MainWindow * main_window,
                     QWidget * parent)
    : QWidget(parent),
      _data_manager{std::move(data_manager)},
      _time_scrollbar{time_scrollbar},
      _main_window{main_window},
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

    std::vector<FeatureProcessingWidget::ProcessedFeatureInfo> active_proc_features =
            _feature_processing_widget->getActiveProcessedFeatures();

    if (active_proc_features.empty() || _training_interval_key.isEmpty() || _selected_outcomes.empty()) {
        std::cerr << "Please select features (and configure transformations), a training data interval, and outcomes" << std::endl;
        return;
    }

    auto training_interval_series = _data_manager->getData<DigitalIntervalSeries>(_training_interval_key.toStdString());
    if (!training_interval_series) {
        std::cerr << "Could not retrieve training interval data: " << _training_interval_key.toStdString() << std::endl;
        return;
    }

    std::vector<std::size_t> training_timestamps = create_timestamps(training_interval_series);
    if (training_timestamps.empty()) {
        std::cerr << "No training timestamps generated from the selected interval: " << _training_interval_key.toStdString() << std::endl;
        return;
    }
    std::sort(training_timestamps.begin(), training_timestamps.end());
    training_timestamps.erase(std::unique(training_timestamps.begin(), training_timestamps.end()), training_timestamps.end());
    std::cout << "Number of unique training timestamps: " << training_timestamps.size() << std::endl;

    std::string feature_matrix_error;
    arma::Mat<double> feature_array = _createFeatureMatrix(active_proc_features, training_timestamps, feature_matrix_error);
    if (!feature_matrix_error.empty()) {
        std::cerr << "Error(s) creating feature matrix:\n"
                  << feature_matrix_error << std::endl;
    }
    if (feature_array.n_cols == 0) {
        std::cerr << "Feature array for training is empty or could not be created. Aborting fit." << std::endl;
        return;
    }
    std::cout << "Training feature array size: " << feature_array.n_rows << " x " << feature_array.n_cols << std::endl;

    // Use existing create_arrays for outcomes for now, assuming it works with a set of outcome keys
    arma::Mat<double> outcome_array = create_arrays(_selected_outcomes, training_timestamps, _data_manager.get());
    if (outcome_array.n_cols == 0 && !training_timestamps.empty()) {// only error if timestamps were available
        std::cerr << "Outcome array for training is empty (0 columns), though timestamps were present. Check outcome data for selected timestamps." << std::endl;
        return;
    } else if (outcome_array.n_cols == 0 && training_timestamps.empty()) {
        // This case is already handled by training_timestamps.empty() check above
    }
    std::cout << "Training outcome array size: " << outcome_array.n_rows << " x " << outcome_array.n_cols << std::endl;

    arma::Row<size_t> labels;
    if (outcome_array.n_rows > 0) {
        labels = arma::conv_to<arma::Row<size_t>>::from(outcome_array.row(0));
    } else if (!training_timestamps.empty()) {// Only error if data was expected
        std::cerr << "Outcome array has 0 rows, but training timestamps exist. Cannot create labels." << std::endl;
        return;
    }
    // ... (rest of _fitModel, including balancing, training, prediction using the new feature_array) ...

    arma::Mat<double> balanced_feature_array;
    arma::Row<size_t> balanced_labels_vec;

    if (labels.empty() && !training_timestamps.empty()) {
        std::cerr << "Labels are empty, cannot proceed with model training." << std::endl;
        return;
    }

    auto const balancing_flag = _class_balancing_widget->isBalancingEnabled();
    std::cout << "Balancing is set to " << balancing_flag << std::endl;

    if (balancing_flag && !labels.empty()) {
        double ratio = _class_balancing_widget->getBalancingRatio();
        if (!balance_training_data_by_subsampling(feature_array, labels, balanced_feature_array, balanced_labels_vec, ratio)) {
            std::cerr << "Data balancing failed. Proceeding with original data, but results may be skewed." << std::endl;
            balanced_feature_array = feature_array;
            balanced_labels_vec = labels;
        }
    } else {
        balanced_feature_array = feature_array;
        balanced_labels_vec = labels;
        if (labels.empty()) std::cout << "Class balancing skipped as labels are empty." << std::endl;
        else
            std::cout << "Class balancing disabled or skipped - using original data distribution." << std::endl;
    }

    if ((balanced_feature_array.n_cols == 0 || balanced_labels_vec.n_elem == 0) && !training_timestamps.empty()) {
        std::cerr << "No data remains after potential balancing. Cannot train model." << std::endl;
        return;
    }
    // If training_timestamps was empty, balanced_feature_array and balanced_labels_vec will also be empty,
    // which is a valid state if no training data was specified, but we should not proceed to train.
    if (training_timestamps.empty()) {
        std::cout << "Skipping model training as no training timestamps were specified." << std::endl;
        // Potentially clear previous model predictions if any?
        return;// Exit _fitModel early
    }

    std::unique_ptr<MLModelParametersBase> model_params_ptr;
    MLParameterWidgetBase * current_param_widget = dynamic_cast<MLParameterWidgetBase *>(ui->stackedWidget->currentWidget());
    if (current_param_widget) {
        model_params_ptr = current_param_widget->getParameters();
    } else {
        std::cerr << "Could not get parameter widget for selected model." << std::endl;
        return;
    }

    if (!model_params_ptr) {
        std::cerr << "Failed to retrieve model parameters from UI." << std::endl;
        return;
    }

    bool trained = _current_selected_model_operation->train(balanced_feature_array, balanced_labels_vec, model_params_ptr.get());
    if (!trained) {
        std::cerr << "Model training failed for " << _current_selected_model_operation->getName() << std::endl;
        return;
    }
    std::cout << "Model trained: " << _current_selected_model_operation->getName() << std::endl;

    arma::Row<size_t> training_predictions;
    if (balanced_feature_array.n_cols > 0) {// only predict if there was data
        bool training_predicted = _current_selected_model_operation->predict(balanced_feature_array, training_predictions);
        if (training_predicted && balanced_labels_vec.n_elem > 0) {
            double const accuracy = 100.0 * (static_cast<double>(arma::accu(training_predictions == balanced_labels_vec))) /
                                    static_cast<double>(balanced_labels_vec.n_elem);
            std::cout << "Training set accuracy (on potentially balanced data) is " << accuracy << "%." << std::endl;
        } else if (balanced_labels_vec.n_elem > 0) {
            std::cerr << "Model prediction on training data failed." << std::endl;
        }
    }

    // --- Determine Prediction Timestamps ---
    int current_time_end_frame;
    if (ui->predict_all_check->isChecked()) {
        current_time_end_frame = _data_manager->getTime()->getTotalFrameCount();
    } else {
        current_time_end_frame = _data_manager->getTime()->getLastLoadedFrame() + 1;
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
    } else {
        std::cout << "Number of prediction timestamps: " << prediction_timestamps.size()
                  << " (Range: " << prediction_timestamps.front() << " to " << prediction_timestamps.back() << ")" << std::endl;

        std::string pred_feature_matrix_error;
        arma::Mat<double> const prediction_feature_array = _createFeatureMatrix(
                active_proc_features, prediction_timestamps, pred_feature_matrix_error);

        if (!pred_feature_matrix_error.empty()) {
            std::cerr << "Error(s) creating prediction feature matrix:\n"
                      << pred_feature_matrix_error << std::endl;
        }

        if (prediction_feature_array.n_cols > 0) {
            arma::Row<size_t> future_predictions;
            bool future_predicted = _current_selected_model_operation->predict(prediction_feature_array, future_predictions);

            if (future_predicted) {
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
            } else {
                std::cerr << "Prediction on new data failed." << std::endl;
            }
        } else {
            std::cout << "No features to predict for the selected prediction timestamps (prediction_feature_array is empty)." << std::endl;
        }
    }
    std::cout << "Exiting _fitModel" << std::endl;
}

// The global create_arrays is still used for outcomes.
// If features are purely handled by _createFeatureMatrix, this could be simplified or renamed.
arma::Mat<double> create_arrays(
        std::unordered_set<std::string> const & data_keys,// Renamed from features to data_keys
        std::vector<std::size_t> & timestamps,
        DataManager * data_manager) {
    std::vector<arma::Mat<double>> component_arrays;

    for (auto const & key: data_keys) {
        auto const data_type = data_manager->getType(key);
        arma::Mat<double> current_array_component;// Can be multiple rows for Point/Tensor

        if (data_type == DM_DataType::Analog) {
            auto analog_series = data_manager->getData<AnalogTimeSeries>(key);
            if (analog_series) current_array_component = convertAnalogTimeSeriesToMlpackArray(analog_series.get(), timestamps).t();// Transpose to N_samples x 1
        } else if (data_type == DM_DataType::DigitalInterval) {
            auto digital_series = data_manager->getData<DigitalIntervalSeries>(key);
            if (digital_series) current_array_component = convertToMlpackArray(digital_series, timestamps).t();// Transpose to N_samples x 1
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
            // Ensure the component matrix has features as rows and samples as columns.
            // The .t() for Analog/Digital already makes it (samples x features=1). For join_cols, we need (features x samples)
            // Let's adjust: convertAnalogTimeSeriesToMlpackArray & convertToMlpackArray (Digital) already return (1 x N_samples)
            // convertToMlpackMatrix (Points) returns (num_point_coords x N_samples)
            // convertTensorDataToMlpackMatrix returns (flattened_tensor_dim x N_samples)
            // So, no transpose should be needed before pushing if we use join_rows.
            // The issue was in _createFeatureMatrix, where I transposed and then used join_rows. It should be join_cols.
            // Let's keep create_arrays consistent: features are rows, samples are columns.
            if (data_type == DM_DataType::Analog || data_type == DM_DataType::DigitalInterval) {
                component_arrays.push_back(current_array_component.t());// Transpose to (N_samples x N_features=1)
            } else {
                component_arrays.push_back(current_array_component);// Already (N_features x N_samples)
            }
        }
    }

    arma::Mat<double> concatenated_array;
    if (!component_arrays.empty()) {
        // All component_arrays should have N_features_i rows and N_samples columns.
        // We want to join them column-wise (meaning each new component adds more columns, representing more features for the same samples).
        // This means each component matrix should be (N_samples x N_features_i).
        // Then we use arma::join_rows to add more features (columns).
        // Let's verify this assumption. Mlpack expects features (dimensions) as rows, and samples as columns.
        // So, if component_arrays[i] is (N_features_i x N_samples), then join_cols is correct.

        concatenated_array = component_arrays[0];
        for (std::size_t i = 1; i < component_arrays.size(); i++) {
            if (component_arrays[i].n_cols == concatenated_array.n_cols) {// Same number of samples
                concatenated_array = arma::join_cols(concatenated_array, component_arrays[i]);
            } else if (component_arrays[i].n_rows == concatenated_array.n_rows) {// This would be if we were adding samples
                                                                                 //This case should not happen if timestamps is common for all
            }
        }
    }
    return concatenated_array;
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
