#ifndef BEHAVIORTOOLBOX_ML_WIDGET_HPP
#define BEHAVIORTOOLBOX_ML_WIDGET_HPP

#include "ClassBalancingWidget/ClassBalancingWidget.hpp"
#include "TimeFrame/interval_data.hpp"
#include "FeatureProcessingWidget/FeatureProcessingWidget.hpp"
#include "MLModelOperation.hpp"
#include "MLModelRegistry.hpp"
#include "ModelMetricsWidget/ModelMetricsWidget.hpp"
#include "Transformations/ITransformation.hpp"
#include "Transformations/TransformationsCommon.hpp"


#include <QWidget>
#include <armadillo>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>


class DataManager;
class DigitalIntervalSeries;
class TableView;
class TimeScrollBar;

namespace Ui {
class ML_Widget;
}

class ML_Widget : public QWidget {
    Q_OBJECT

public:
    ML_Widget(std::shared_ptr<DataManager> data_manager,
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
    // Table-based ML helpers
    void _populateAvailableTablesAndColumns();
    void _onSelectedTableChanged(QString const & table_id);
    arma::Mat<double> _buildFeatureMatrixFromTable(std::shared_ptr<TableView> const & table,
                                                   std::vector<std::string> const & feature_columns,
                                                   std::vector<size_t> & kept_row_indices) const;
    std::optional<arma::Row<size_t>> _buildLabelsFromTable(std::shared_ptr<TableView> const & table,
                                                           std::string const & label_column,
                                                           std::vector<size_t> const & kept_row_indices) const;
    std::vector<size_t> _applyMasksFromTable(std::shared_ptr<TableView> const & table,
                                             std::vector<std::string> const & mask_columns,
                                             std::vector<size_t> const & candidate_rows) const;

    arma::Mat<double> _createFeatureMatrix(
            std::vector<FeatureProcessingWidget::ProcessedFeatureInfo> const & processed_features,
            std::vector<std::size_t> const & timestamps,
            std::string & error_message) const;

    arma::Mat<double> _removeNaNColumns(arma::Mat<double> const & matrix, std::vector<std::size_t> & timestamps) const;
    arma::Mat<double> _zScoreNormalizeFeatures(arma::Mat<double> const & matrix,
                                               std::vector<FeatureProcessingWidget::ProcessedFeatureInfo> const & processed_features) const;

    /**
     * @brief Prepare training data including feature matrix and outcome arrays
     * @param active_proc_features Vector of processed feature information
     * @param training_timestamps Output vector of training timestamps
     * @param feature_array Output feature matrix for training
     * @param outcome_array Output outcome matrix for training
     * @return std::optional<arma::Row<size_t>> Labels row if successful, std::nullopt if failed
     */
    std::optional<arma::Row<size_t>> _prepareTrainingData(
            std::vector<FeatureProcessingWidget::ProcessedFeatureInfo> const & active_proc_features,
            std::vector<std::size_t> & training_timestamps,
            arma::Mat<double> & feature_array,
            arma::Mat<double> & outcome_array) const;

    /**
     * @brief Train the machine learning model with prepared data
     * @param feature_array Input feature matrix
     * @param labels Input labels
     * @param balanced_feature_array Output balanced feature matrix
     * @param balanced_labels Output balanced labels
     * @return bool True if training successful, false otherwise
     */
    bool _trainModel(arma::Mat<double> const & feature_array,
                     arma::Row<size_t> const & labels,
                     arma::Mat<double> & balanced_feature_array,
                     arma::Row<size_t> & balanced_labels);

    /**
     * @brief Predict labels for new data not included in training
     * @param active_proc_features Vector of processed feature information
     * @param training_timestamps Training timestamps to exclude from prediction
     * @return bool True if prediction successful, false otherwise
     */
    bool _predictNewData(std::vector<FeatureProcessingWidget::ProcessedFeatureInfo> const & active_proc_features,
                         std::vector<std::size_t> const & training_timestamps);

    std::shared_ptr<DataManager> _data_manager;
    TimeScrollBar * _time_scrollbar;
    Ui::ML_Widget * ui;

    std::unique_ptr<MLModelRegistry> _ml_model_registry;
    MLModelOperation * _current_selected_model_operation = nullptr;
    std::map<std::string, int> _model_name_to_widget_index;

    QString _training_interval_key;
    std::unordered_set<std::string> _selected_outcomes;

    FeatureProcessingWidget * _feature_processing_widget = nullptr; // removed from UI; kept for legacy path if needed
    ClassBalancingWidget * _class_balancing_widget;
    ModelMetricsWidget * _model_metrics_widget;

    std::map<WhiskerTransformations::TransformationType, std::unique_ptr<ITransformation>> _transformation_registry;

    // Table-based ML state
    QString _selected_table_id;
    std::vector<std::string> _selected_feature_columns;
    std::vector<std::string> _selected_mask_columns;
    std::string _selected_label_column;
};

arma::Mat<double> create_arrays(std::unordered_set<std::string> const & features,
                                std::vector<std::size_t> & timestamps,
                                DataManager * data_manager);

std::vector<std::size_t> create_timestamps(std::shared_ptr<DigitalIntervalSeries> & series);
std::vector<std::size_t> create_timestamps(std::vector<Interval> & intervals);

#endif//BEHAVIORTOOLBOX_ML_WIDGET_HPP
