#include "ML_Widget.hpp"
#include "ui_ML_Widget.h"

#include "DataManager.hpp"

//https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots
#include "mlpack_conversion.hpp"
#define slots Q_SLOTS

#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/utils/armadillo_wrap/analog_armadillo.hpp"
#include "ML_Naive_Bayes_Widget/ML_Naive_Bayes_Widget.hpp"
#include "ML_Random_Forest_Widget/ML_Random_Forest_Widget.hpp"
#include "TimeFrame.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"

#include "MLModelOperation.hpp"
#include "MLModelRegistry.hpp"
#include "MLParameterWidgetBase.hpp"

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

    //Feature Table Widget
    ui->feature_table_widget->setColumns({"Feature", "Enabled", "Type"});
    ui->feature_table_widget->setTypeFilter({DM_DataType::Analog, DM_DataType::DigitalInterval, DM_DataType::Points, DM_DataType::Tensor});

    ui->feature_table_widget->setDataManager(_data_manager);

    connect(ui->feature_table_widget, &Feature_Table_Widget::featureSelected, this, &ML_Widget::_handleFeatureSelected);
    connect(ui->feature_table_widget, &Feature_Table_Widget::addFeature, this, [this](QString const & feature) {
        ML_Widget::_addFeatureToModel(feature, true);
    });
    connect(ui->feature_table_widget, &Feature_Table_Widget::removeFeature, this, [this](QString const & feature) {
        ML_Widget::_addFeatureToModel(feature, false);
    });

    // Connect new trainingIntervalComboBox
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
    connect(ui->fit_button, &QPushButton::clicked, this, &ML_Widget::_fitModel);// Initialize class balancing widget
    _class_balancing_widget = ui->class_balancing_widget;
    connect(_class_balancing_widget, &ClassBalancingWidget::balancingSettingsChanged, this, &ML_Widget::_updateClassDistribution);

    // DataManager Observer for training interval ComboBox
    _data_manager->addObserver([this]() {
        _populateTrainingIntervalComboBox();
    });
    _populateTrainingIntervalComboBox(); // Initial population
}

ML_Widget::~ML_Widget() {
    delete ui;
}

void ML_Widget::openWidget() {
    std::cout << "ML Widget Opened" << std::endl;

    ui->feature_table_widget->populateTable();
    ui->outcome_table_widget->populateTable();

    this->show();
}

void ML_Widget::closeEvent(QCloseEvent * event) {
    std::cout << "Close event detected" << std::endl;
}

void ML_Widget::_handleFeatureSelected(QString const & feature) {
    _highlighted_available_feature = feature;
}

void ML_Widget::_addFeatureToModel(QString const & feature, bool enabled) {
    if (enabled) {
        //_plotSelectedFeature(feature.toStdString());
        _selected_features.insert(feature.toStdString());
    } else {
        _removeSelectedFeature(feature.toStdString());
    }
}

void ML_Widget::_removeSelectedFeature(std::string const & key) {

    if (auto iter = _selected_features.find(key); iter != _selected_features.end())
        _selected_features.erase(iter);
}

void ML_Widget::_handleOutcomeSelected(QString const & feature) {
    return;
}

void ML_Widget::_addOutcomeToModel(QString const & feature, bool enabled) {
    if (enabled) {
        //_plotSelectedFeature(feature.toStdString());
        _selected_outcomes.insert(feature.toStdString());
    } else {
        _removeSelectedOutcome(feature.toStdString());
    }
}

void ML_Widget::_removeSelectedOutcome(std::string const & key) {
    if (_data_manager->getType(key) == DM_DataType::Analog) {
        //ui->openGLWidget->removeAnalogTimeSeries(key);
    } else if (_data_manager->getType(key) == DM_DataType::DigitalEvent) {
        //ui->openGLWidget->removeDigitalEventSeries(key);
    } else if (_data_manager->getType(key) == DM_DataType::DigitalInterval) {
        //ui->openGLWidget->removeDigitalIntervalSeries(key);
    } else {
        std::cout << "Feature type not supported" << std::endl;
    }
    if (auto iter = _selected_outcomes.find(key); iter != _selected_outcomes.end())
        _selected_outcomes.erase(iter);
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
            ui->stackedWidget->setCurrentIndex(0);
            ui->fit_button->setEnabled(false);
        }
    } else {
        std::cerr << "Unsupported Model Type Selected: " << model_type << std::endl;
        ui->stackedWidget->setCurrentIndex(0);
        ui->fit_button->setEnabled(false);
    }
}

void ML_Widget::_fitModel() {
    if (!_current_selected_model_operation) {
        std::cerr << "No model operation selected." << std::endl;
        return;
    }
    if (_selected_features.empty() || _training_interval_key.isEmpty() || _selected_outcomes.empty()) {
        std::cerr << "Please select features, a training data interval, and outcomes" << std::endl;
        return;
    }

    auto masks = _data_manager->getData<DigitalIntervalSeries>(_training_interval_key.toStdString());
    if (!masks) {
        std::cerr << "Could not retrieve training interval data: " << _training_interval_key.toStdString() << std::endl;
        return;
    }
    auto timestamps = create_timestamps(masks);

    auto feature_array = create_arrays(_selected_features, timestamps, _data_manager.get());
    std::cout << "Feature array size: " << feature_array.n_rows << " x " << feature_array.n_cols << std::endl;

    auto outcome_array = create_arrays(_selected_outcomes, timestamps, _data_manager.get());

    std::cout << "Outcome array size: " << outcome_array.n_rows << " x " << outcome_array.n_cols << std::endl;
    arma::Row<size_t> labels = arma::conv_to<arma::Row<size_t>>::from(outcome_array.row(0));// Assuming outcome is single row

    //_updateClassDistribution(); // Update display before balancing

    arma::Mat<double> balanced_feature_array;
    arma::Row<size_t> balanced_labels_vec;

    auto const balancing_flag = _class_balancing_widget->isBalancingEnabled();

    std::cout << "Balancing is set to " << balancing_flag << std::endl;

    if (balancing_flag) {
        double ratio = _class_balancing_widget->getBalancingRatio(); // Get ratio
        if (!balance_training_data_by_subsampling(feature_array, labels, balanced_feature_array, balanced_labels_vec, ratio)) {
            std::cerr << "Data balancing failed. Proceeding with original data, but results may be skewed." << std::endl;
            balanced_feature_array = feature_array;
            balanced_labels_vec = labels;
        }
    } else {
        balanced_feature_array = feature_array;
        balanced_labels_vec = labels;
        std::cout << "Class balancing disabled - using original data distribution." << std::endl;
    }

    if (balanced_feature_array.n_cols == 0 || balanced_labels_vec.n_elem == 0) {
        std::cerr << "No data remains after attempting to balance. Cannot train model." << std::endl;
        return;
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
    // Use balanced data for training
    bool trained = _current_selected_model_operation->train(balanced_feature_array, balanced_labels_vec, model_params_ptr.get());
    if (!trained) {
        std::cerr << "Model training failed for " << _current_selected_model_operation->getName() << std::endl;
        return;
    }
    std::cout << "Model trained: " << _current_selected_model_operation->getName() << std::endl;

    arma::Row<size_t> predictions;
    bool predicted = _current_selected_model_operation->predict(balanced_feature_array, predictions);

    if (!predicted) {
        std::cerr << "Model prediction (on training data) failed." << std::endl;
        return;
    }

    std::cout << "predictions have max" << arma::max(predictions) << std::endl;

    double const accuracy = 100.0 * (static_cast<double>(arma::accu(predictions == balanced_labels_vec))) /
                            static_cast<double>(balanced_labels_vec.n_elem);
    std::cout << "After training " << _current_selected_model_operation->getName()
              << ", training set accuracy (on balanced data) is " << accuracy << "%." << std::endl;

    int current_time_end_frame;
    if (ui->predict_all_check->isChecked()) {
        current_time_end_frame = _data_manager->getTime()->getTotalFrameCount();
    } else {
        current_time_end_frame = _data_manager->getTime()->getLastLoadedFrame();
    }

    int prediction_start_frame = 0;
    if (masks && !masks->getDigitalIntervalSeries().empty()) {
        auto const & training_intervals = masks->getDigitalIntervalSeries();
        if (!training_intervals.empty()) {
            prediction_start_frame = training_intervals.back().end;
        }
    } else if (!timestamps.empty()) {
        prediction_start_frame = timestamps.back() + 1;
    }

    if (prediction_start_frame >= current_time_end_frame) {
        std::cout << "No new frames to predict after training data. Start frame: " << prediction_start_frame
                  << ", End frame: " << current_time_end_frame << std::endl;
    } else {
        std::vector<Interval> prediction_interval_vec = {Interval{
                static_cast<std::size_t>(prediction_start_frame),
                static_cast<std::size_t>(current_time_end_frame)}};

        auto prediction_timestamps = create_timestamps(prediction_interval_vec);

        std::cout << "The length of prediction timestamps is " << prediction_timestamps.size() << std::endl;
        std::cout << "They range from " << prediction_timestamps[0] << " to " << prediction_timestamps.back() << std::endl;

        if (!prediction_timestamps.empty()) {
            arma::Mat<double> const prediction_feature_array = create_arrays(
                    _selected_features,
                    prediction_timestamps,
                    _data_manager.get());

            if (prediction_feature_array.n_cols > 0) {
                arma::Row<size_t> future_predictions;
                bool future_predicted = _current_selected_model_operation->predict(prediction_feature_array, future_predictions);

                if (future_predicted) {
                    auto prediction_vec = copyMatrixRowToVector<size_t>(future_predictions);

                    std::cout << "Range of predictions. Max: " << *std::max(prediction_vec.begin(), prediction_vec.end()) << std::endl;
                    std::cout << "Min: " << *std::min_element(prediction_vec.begin(), prediction_vec.end()) << std::endl;
                    for (auto const & key: _selected_outcomes) {
                        auto outcome_series = _data_manager->getData<DigitalIntervalSeries>(key);
                        if (outcome_series) {
                            outcome_series->setEventsAtTimes(prediction_timestamps, prediction_vec);
                            std::cout << "Predictions applied to: " << key << std::endl;
                        } else {
                            std::cerr << "Could not get outcome series for key: " << key << std::endl;
                        }
                    }
                } else {
                    std::cerr << "Prediction on new data failed." << std::endl;
                }
            } else {
                std::cout << "No features to predict for the selected time range." << std::endl;
            }
        } else {
            std::cout << "No timestamps generated for prediction interval." << std::endl;
        }
    }
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


arma::Mat<double> create_arrays(
        std::unordered_set<std::string> const & features,
        std::vector<std::size_t> & timestamps,
        DataManager * data_manager) {
    //Create the input and output arrays

    std::vector<arma::Mat<double>> feature_arrays;

    for (auto const & feature: features) {
        auto const feature_type = data_manager->getType(feature);

        if (feature_type == DM_DataType::Analog) {
            auto analog_series = data_manager->getData<AnalogTimeSeries>(feature);
            arma::Row<double> const array = convertAnalogTimeSeriesToMlpackArray(analog_series.get(), timestamps);
            feature_arrays.push_back(array);
        } else if (feature_type == DM_DataType::DigitalInterval) {
            auto digital_series = data_manager->getData<DigitalIntervalSeries>(feature);
            arma::Row<double> const array = convertToMlpackArray(digital_series, timestamps);
            feature_arrays.push_back(array);
        } else if (feature_type == DM_DataType::Points) {
            auto point_data = data_manager->getData<PointData>(feature);
            arma::Mat<double> const array = convertToMlpackMatrix(point_data, timestamps);
            feature_arrays.push_back(array);
        } else if (feature_type == DM_DataType::Tensor) {
            auto tensor_data = data_manager->getData<TensorData>(feature);
            arma::Mat<double> const array = convertTensorDataToMlpackMatrix(*tensor_data, timestamps);
            feature_arrays.push_back(array);
        } else {
            std::cerr << "Unsupported feature type: " << convert_data_type_to_string(feature_type) << std::endl;
        }
    }

    // Concatenate all feature arrays column-wise
    arma::Mat<double> concatenated_array;
    if (!feature_arrays.empty()) {
        concatenated_array = feature_arrays[0];

        for (std::size_t i = 1; i < feature_arrays.size(); i++) {
            concatenated_array = arma::join_cols(concatenated_array, feature_arrays[i]);
        }
    }

    return concatenated_array;
}

void ML_Widget::_onTrainingIntervalChanged(const QString& intervalKey) {
    _training_interval_key = intervalKey;
    _updateClassDistribution(); // Update distribution when interval changes
    // Potentially trigger other updates if needed
}

void ML_Widget::_populateTrainingIntervalComboBox() {
    QString currentSelection = ui->trainingIntervalComboBox->currentText();
    ui->trainingIntervalComboBox->blockSignals(true);
    ui->trainingIntervalComboBox->clear();

    std::vector<std::string> intervalKeys = _data_manager->getKeys<DigitalIntervalSeries>();
    ui->trainingIntervalComboBox->addItem(""); // Add an empty item for no selection
    for (const auto& key : intervalKeys) {
        ui->trainingIntervalComboBox->addItem(QString::fromStdString(key));
    }

    if (intervalKeys.empty()){
        _training_interval_key.clear();
    }

    // Try to restore previous selection
    int index = ui->trainingIntervalComboBox->findText(currentSelection);
    if (index != -1) {
        ui->trainingIntervalComboBox->setCurrentIndex(index);
        _training_interval_key = currentSelection; // Ensure this is set if restored
    } else if (!intervalKeys.empty()) {
        // If previous selection not found, and list is not empty, select first valid item
        // but only if current _training_interval_key is no longer valid or empty.
        bool currentKeyIsValid = false;
        for(const auto& key : intervalKeys) {
            if (QString::fromStdString(key) == _training_interval_key) {
                currentKeyIsValid = true;
                break;
            }
        }
        if (!currentKeyIsValid && ui->trainingIntervalComboBox->count() > 1) { // Count > 1 because of empty item
             ui->trainingIntervalComboBox->setCurrentIndex(1); // Select first actual item
             _training_interval_key = ui->trainingIntervalComboBox->currentText();
        } else if (!currentKeyIsValid) {
            _training_interval_key.clear(); // No valid selection possible
        }
        // If currentKeyIsValid, _training_interval_key remains as is, and ComboBox reflects it if found, or shows empty.

    } else {
         _training_interval_key.clear(); // List is empty, no selection possible
    }

    ui->trainingIntervalComboBox->blockSignals(false);
    _updateClassDistribution(); // Update distribution after populating

}

void ML_Widget::_updateClassDistribution() {
    if (_selected_features.empty() || _training_interval_key.isEmpty() || _selected_outcomes.empty()) {
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
        auto timestamps = create_timestamps(current_mask_series);
        if (timestamps.empty()) {
            _class_balancing_widget->clearClassDistribution();
             std::cout << "No timestamps generated from training interval for distribution update." << std::endl;
            return;
        }

        auto outcome_array = create_arrays(_selected_outcomes, timestamps, _data_manager.get());
        if (outcome_array.empty()){
             _class_balancing_widget->clearClassDistribution();
            return;
        }
        arma::Row<size_t> labels = arma::conv_to<arma::Row<size_t>>::from(outcome_array.row(0));

        std::map<size_t, size_t> class_counts;
        for (size_t i = 0; i < labels.n_elem; ++i) {
            class_counts[labels[i]]++;
        }

        if (class_counts.empty()) {
            _class_balancing_widget->clearClassDistribution();
            return;
        }

        QString distributionText = "Original: ";
        bool first = true;
        for (auto const& [label_val, count] : class_counts) {
            if (!first) distributionText += ", ";
            distributionText += QString("Class %1: %L2").arg(label_val).arg(count);
            first = false;
        }

        if (_class_balancing_widget->isBalancingEnabled()) {
            size_t min_class_count = std::numeric_limits<size_t>::max();
            bool found_any_class_with_samples = false;
            for (auto const& [label_val, count] : class_counts) {
                if (count > 0) {
                    found_any_class_with_samples = true;
                    if (count < min_class_count) {
                        min_class_count = count;
                    }
                }
            }
            if (!found_any_class_with_samples) min_class_count = 0; 

            double ratio = _class_balancing_widget->getBalancingRatio();
            size_t target_max_samples = (min_class_count > 0) ? static_cast<size_t>(min_class_count * ratio) : 0;
            if (target_max_samples == 0 && min_class_count > 0 && ratio >= 1.0) target_max_samples = 1; 

            distributionText += "\nBalanced: ";
            first = true;
            for (auto const& [label_val, count] : class_counts) {
                if (!first) distributionText += ", ";
                size_t balanced_count = 0;
                if (count > 0) { // Only consider classes that originally had samples
                    balanced_count = std::min(count, target_max_samples);
                     if (balanced_count == 0 && target_max_samples > 0) balanced_count = 1; 
                }
                distributionText += QString("Class %1: %L2").arg(label_val).arg(balanced_count);
                first = false;
            }
        }
        _class_balancing_widget->updateClassDistribution(distributionText);

    } catch (const std::exception& e) {
        _class_balancing_widget->clearClassDistribution();
        std::cerr << "Error updating class distribution: " << e.what() << std::endl;
    }
}
