#ifndef BEHAVIORTOOLBOX_ML_WIDGET_HPP
#define BEHAVIORTOOLBOX_ML_WIDGET_HPP

#include "ClassBalancingWidget/ClassBalancingWidget.hpp"
#include "DataManager/DigitalTimeSeries/interval_data.hpp"
#include "FeatureProcessingWidget/FeatureProcessingWidget.hpp"
#include "MLModelOperation.hpp"
#include "MLModelRegistry.hpp"
#include "Transformations/ITransformation.hpp"
#include "Transformations/TransformationsCommon.hpp"


#include <QWidget>
#include <armadillo>

#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>


class DataManager;
class DigitalIntervalSeries;
class MainWindow;
class TimeScrollBar;

namespace Ui {
class ML_Widget;
}

class ML_Widget : public QWidget {
    Q_OBJECT

public:
    ML_Widget(std::shared_ptr<DataManager> data_manager,
              TimeScrollBar * time_scrollbar,
              MainWindow * main_window,
              QWidget * parent = nullptr);

    ~ML_Widget() override;

    void openWidget();

protected:
    void closeEvent(QCloseEvent * event) override;

private slots:
    void _onTrainingIntervalChanged(QString const & intervalKey);

    void _handleOutcomeSelected(QString const & feature);
    void _addOutcomeToModel(QString const & feature, bool enabled);
    void _removeSelectedOutcome(std::string const & key);

    void _selectModelType(QString const & model_type);
    void _fitModel();
    void _updateClassDistribution();
    void _populateTrainingIntervalComboBox();

private:
    arma::Mat<double> _createFeatureMatrix(
            std::vector<FeatureProcessingWidget::ProcessedFeatureInfo> const & processed_features,
            std::vector<std::size_t> const & timestamps,
            std::string & error_message) const;
    
    arma::Mat<double> _removeNaNColumns(arma::Mat<double> const & matrix, std::vector<std::size_t> & timestamps) const;
    arma::Mat<double> _zScoreNormalizeFeatures(arma::Mat<double> const & matrix, 
            std::vector<FeatureProcessingWidget::ProcessedFeatureInfo> const & processed_features) const;

    std::shared_ptr<DataManager> _data_manager;
    TimeScrollBar * _time_scrollbar;
    MainWindow * _main_window;
    Ui::ML_Widget * ui;

    std::unique_ptr<MLModelRegistry> _ml_model_registry;
    MLModelOperation * _current_selected_model_operation = nullptr;
    std::map<std::string, int> _model_name_to_widget_index;

    QString _training_interval_key;
    std::unordered_set<std::string> _selected_outcomes;

    FeatureProcessingWidget * _feature_processing_widget;
    ClassBalancingWidget * _class_balancing_widget;

    std::map<WhiskerTransformations::TransformationType, std::unique_ptr<ITransformation>> _transformation_registry;
};

arma::Mat<double> create_arrays(std::unordered_set<std::string> const & features,
                                std::vector<std::size_t> & timestamps,
                                DataManager * data_manager);

std::vector<std::size_t> create_timestamps(std::shared_ptr<DigitalIntervalSeries> & series);
std::vector<std::size_t> create_timestamps(std::vector<Interval> & intervals);

#endif//BEHAVIORTOOLBOX_ML_WIDGET_HPP
