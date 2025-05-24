#ifndef BEHAVIORTOOLBOX_ML_WIDGET_HPP
#define BEHAVIORTOOLBOX_ML_WIDGET_HPP

#include "DataManager/DigitalTimeSeries/interval_data.hpp"

#include <QWidget>

#include <mlpack/core.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>
#include <map>

#include "MLModelRegistry.hpp"
#include "MLModelOperation.hpp"
#include "ClassBalancingWidget/ClassBalancingWidget.hpp"

class DataManager;
class DigitalIntervalSeries;
class MainWindow;
class QTableWidget;
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
    void _handleFeatureSelected(QString const & feature);
    void _addFeatureToModel(QString const & feature, bool enabled);
    void _removeSelectedFeature(std::string const & key);

    void _handleMaskSelected(QString const & feature);
    void _addMaskToModel(QString const & feature, bool enabled);
    void _removeSelectedMask(std::string const & key);

    void _handleOutcomeSelected(QString const & feature);
    void _addOutcomeToModel(QString const & feature, bool enabled);
    void _removeSelectedOutcome(std::string const & key);

        void _selectModelType(QString const & model_type);    void _fitModel();        void _updateClassDistribution();

private:
    std::shared_ptr<DataManager> _data_manager;
    TimeScrollBar * _time_scrollbar;
    MainWindow * _main_window;
    Ui::ML_Widget * ui;
    QString _highlighted_available_feature;

    std::unique_ptr<MLModelRegistry> _ml_model_registry;
    MLModelOperation* _current_selected_model_operation = nullptr;
    std::map<std::string, int> _model_name_to_widget_index;

    std::unordered_set<std::string> _selected_features;
    std::unordered_set<std::string> _selected_masks;
    std::unordered_set<std::string> _selected_outcomes;
    ClassBalancingWidget* _class_balancing_widget;

    arma::Mat<double> _features;
    arma::Mat<double> _outcomes;
};

arma::Mat<double> create_arrays(std::unordered_set<std::string> const & features,
                                std::vector<std::size_t> & timestamps,
                                DataManager * data_manager);

std::vector<std::size_t> create_timestamps(std::shared_ptr<DigitalIntervalSeries> & series);
std::vector<std::size_t> create_timestamps(std::vector<Interval> & intervals);

#endif//BEHAVIORTOOLBOX_ML_WIDGET_HPP
