
#include "ML_Widget.hpp"
#include "ui_ML_Widget.h"

#include "DataManager.hpp"

//https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots
#include "mlpack_conversion.hpp"
#define slots Q_SLOTS

#include "ML_Random_Forest_Widget/ML_Random_Forest_Widget.hpp"
#include "TimeFrame.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"

#include <QFileDialog>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>

#include <fstream>
#include <iostream>

ML_Widget::ML_Widget(std::shared_ptr<DataManager> data_manager,
                     TimeScrollBar* time_scrollbar,
                     MainWindow* mainwindow,
                     QWidget *parent) :
        QMainWindow(parent),
        _data_manager{data_manager},
        _time_scrollbar{time_scrollbar},
        _main_window{mainwindow},
        ui(new Ui::ML_Widget)
{
    ui->setupUi(this);

    ui->stackedWidget->addWidget(new ML_Random_Forest_Widget(_data_manager));

    //Feature Table Widget
    ui->feature_table_widget->setColumns({"Feature", "Enabled", "Type"});
    ui->feature_table_widget->setTypeFilter({"AnalogTimeSeries", "DigitalIntervalSeries","PointData", "TensorData"});

    ui->feature_table_widget->setDataManager(_data_manager);

    connect(ui->feature_table_widget, &Feature_Table_Widget::featureSelected, this, &ML_Widget::_handleFeatureSelected);
    connect(ui->feature_table_widget, &Feature_Table_Widget::addFeature, this, [this](const QString& feature) {
        ML_Widget::_addFeatureToModel(feature, true);
    });
    connect(ui->feature_table_widget, &Feature_Table_Widget::removeFeature, this, [this](const QString& feature) {
        ML_Widget::_addFeatureToModel(feature, false);
    });

    //Mask Table Widget
    ui->mask_table_widget->setColumns({"Feature", "Enabled", "Type"});
    ui->mask_table_widget->setTypeFilter({"DigitalIntervalSeries"});

    ui->mask_table_widget->setDataManager(_data_manager);

    connect(ui->mask_table_widget, &Feature_Table_Widget::featureSelected, this, &ML_Widget::_handleMaskSelected);
    connect(ui->mask_table_widget, &Feature_Table_Widget::addFeature, this, [this](const QString& feature) {
        ML_Widget::_addMaskToModel(feature, true);
    });
    connect(ui->mask_table_widget, &Feature_Table_Widget::removeFeature, this, [this](const QString& feature) {
        ML_Widget::_addMaskToModel(feature, false);
    });

    //Outcome Table Widget
    ui->outcome_table_widget->setColumns({"Feature", "Enabled", "Type"});
    ui->outcome_table_widget->setTypeFilter({"AnalogTimeSeries", "DigitalIntervalSeries","PointData", "TensorData"});

    ui->outcome_table_widget->setDataManager(_data_manager);

    connect(ui->outcome_table_widget, &Feature_Table_Widget::featureSelected, this, &ML_Widget::_handleOutcomeSelected);
    connect(ui->outcome_table_widget, &Feature_Table_Widget::addFeature, this, [this](const QString& feature) {
        ML_Widget::_addOutcomeToModel(feature, true);
    });
    connect(ui->outcome_table_widget, &Feature_Table_Widget::removeFeature, this, [this](const QString& feature) {
        ML_Widget::_addOutcomeToModel(feature, false);
    });

    connect(ui->model_select_combo, &QComboBox::currentTextChanged, this, &ML_Widget::_selectModelType);
    connect(ui->fit_button, &QPushButton::clicked, this, &ML_Widget::_fitModel);
}

ML_Widget::~ML_Widget() {
    delete ui;
}

void ML_Widget::openWidget() {
    std::cout << "ML Widget Opened" << std::endl;

    ui->feature_table_widget->populateTable();
    ui->mask_table_widget->populateTable();
    ui->outcome_table_widget->populateTable();

    this->show();
}

void ML_Widget::closeEvent(QCloseEvent *event) {
    std::cout << "Close event detected" << std::endl;
}

void ML_Widget::_handleFeatureSelected(const QString& feature)
{
    _highlighted_available_feature = feature;
}

void ML_Widget::_addFeatureToModel(const QString& feature, bool enabled)
{
    if (enabled) {
        //_plotSelectedFeature(feature.toStdString());
        _selected_features.insert(feature.toStdString());
    } else {
        _removeSelectedFeature(feature.toStdString());
    }
}

void ML_Widget::_removeSelectedFeature(const std::string key) {

    if (auto iter = _selected_features.find(key); iter != _selected_features.end())
        _selected_features.erase(iter);
}

void ML_Widget::_handleMaskSelected(const QString& feature)
{
    return;
}

void ML_Widget::_addMaskToModel(const QString& feature, bool enabled)
{
    if (enabled) {
        //_plotSelectedFeature(feature.toStdString());
        _selected_masks.insert(feature.toStdString());
    } else {
        _removeSelectedMask(feature.toStdString());
    }
}

void ML_Widget::_removeSelectedMask(const std::string key) {
    if (_data_manager->getType(key) == "DigitalIntervalSeries") {
        //ui->openGLWidget->removeDigitalIntervalSeries(key);
    } else {
        std::cout << "Feature type not supported" << std::endl;
    }
    if (auto iter = _selected_masks.find(key); iter != _selected_masks.end())
        _selected_masks.erase(iter);
}

void ML_Widget::_handleOutcomeSelected(const QString& feature)
{
    return;
}

void ML_Widget::_addOutcomeToModel(const QString& feature, bool enabled)
{
    if (enabled) {
        //_plotSelectedFeature(feature.toStdString());
        _selected_outcomes.insert(feature.toStdString());
    } else {
        _removeSelectedOutcome(feature.toStdString());
    }
}

void ML_Widget::_removeSelectedOutcome(const std::string key) {
    if (_data_manager->getType(key) == "AnalogTimeSeries") {
        //ui->openGLWidget->removeAnalogTimeSeries(key);
    } else if (_data_manager->getType(key) == "PointData") {
        //ui->openGLWidget->removeDigitalEventSeries(key);
    } else if (_data_manager->getType(key) == "DigitalIntervalSeries") {
        //ui->openGLWidget->removeDigitalIntervalSeries(key);
    } else {
        std::cout << "Feature type not supported" << std::endl;
    }
    if (auto iter = _selected_outcomes.find(key); iter != _selected_outcomes.end())
        _selected_outcomes.erase(iter);
}

void ML_Widget::_selectModelType(const QString& model_type)
{
    if (model_type == "Random Forest")
    {
        ui->stackedWidget->setCurrentIndex(0);
    } else {
        std::cout << "Unsupported Model Type Selected" << std::endl;
    }
}

void ML_Widget::_fitModel()
{

    if (_selected_features.empty() || _selected_masks.empty() || _selected_outcomes.empty())
    {
        std::cerr << "Please select features, masks, and outcomes" << std::endl;
        return;
    }

    if (_selected_masks.size() > 1)
    {
        std::cerr << "Only one mask is supported" << std::endl;
        return;
    }
    auto masks = _data_manager->getData<DigitalIntervalSeries>(*_selected_masks.begin());
    auto timestamps = create_timestamps(masks);

    auto feature_array = create_arrays(_selected_features, timestamps, _data_manager);

    std::cout << "Feature array size: " << feature_array.n_rows << " x " << feature_array.n_cols << std::endl;
}

std::vector<std::size_t> create_timestamps(std::shared_ptr<DigitalIntervalSeries>& series)
{
    std::vector<std::size_t> timestamps;
    auto intervals = series->getDigitalIntervalSeries();
    for (auto interval : intervals)
    {
        //I want to generate timestamps for each value between interval.start and interval.end
        for (std::size_t i = interval.start; i < interval.end; i++)
        {
            timestamps.push_back(i);
        }
    }

    return timestamps;
}

arma::Mat<double> create_arrays(
        std::unordered_set<std::string> features,
        std::vector<std::size_t>& timestamps,
        std::shared_ptr<DataManager> data_manager)
{
    //Create the input and output arrays

    std::vector<arma::Mat<double>> feature_arrays;

    for (const auto& feature : features) {
        std::string feature_type = data_manager->getType(feature);

        if (feature_type == "AnalogTimeSeries") {
            auto analog_series = data_manager->getData<AnalogTimeSeries>(feature);
            arma::Row<double> array = convertAnalogTimeSeriesToMlpackArray(analog_series, timestamps);
            feature_arrays.push_back(array);
        } else if (feature_type == "DigitalIntervalSeries") {
            auto digital_series = data_manager->getData<DigitalIntervalSeries>(feature);
            arma::Row<double> array = convertToMlpackArray(digital_series, timestamps);
            feature_arrays.push_back(array);
        } else if (feature_type == "PointData") {
            auto point_data = data_manager->getData<PointData>(feature);
            arma::Mat<double> array = convertToMlpackMatrix(point_data, timestamps);
            feature_arrays.push_back(array);
        } else if (feature_type == "TensorData") {
            auto tensor_data = data_manager->getData<TensorData>(feature);
            arma::Mat<double> array = convertTensorDataToMlpackMatrix(*tensor_data, timestamps);
            feature_arrays.push_back(array);
        } else {
            std::cerr << "Unsupported feature type: " << feature_type << std::endl;
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

