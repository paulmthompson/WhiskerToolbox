#ifndef MODELMETRICSWIDGET_HPP
#define MODELMETRICSWIDGET_HPP

#include <QWidget>
#include <armadillo>

#include <cstddef>
#include <string>

namespace Ui {
class ModelMetricsWidget;
}

/**
 * @brief Enum representing different types of machine learning models and their metrics
 */
enum class ModelMetricType {
    BinaryClassification,
    MultiClassification, // For future extension
    Regression          // For future extension
};

/**
 * @brief Structure to hold binary classification metrics
 */
struct BinaryClassificationMetrics {
    double accuracy = 0.0;      ///< Overall correctness: (TP + TN) / (TP + TN + FP + FN)
    double sensitivity = 0.0;   ///< True Positive Rate: TP / (TP + FN)
    double specificity = 0.0;   ///< True Negative Rate: TN / (TN + FP)
    double dice_score = 0.0;    ///< F1 Score: 2*TP / (2*TP + FP + FN)
    
    // Confusion matrix components
    std::size_t true_positives = 0;
    std::size_t true_negatives = 0;
    std::size_t false_positives = 0;
    std::size_t false_negatives = 0;
    
    /**
     * @brief Check if metrics have been calculated (non-zero total predictions)
     * @return bool True if metrics are available, false otherwise
     */
    [[nodiscard]] bool isValid() const {
        return (true_positives + true_negatives + false_positives + false_negatives) > 0;
    }
    
    /**
     * @brief Get total number of predictions
     * @return std::size_t Total number of predictions
     */
    [[nodiscard]] std::size_t getTotalPredictions() const {
        return true_positives + true_negatives + false_positives + false_negatives;
    }
};

/**
 * @brief Widget for displaying machine learning model performance metrics
 * 
 * This widget provides a user interface for displaying various machine learning 
 * model performance metrics. Currently supports binary classification metrics
 * with design for future extension to multi-class classification and regression.
 */
class ModelMetricsWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructor for ModelMetricsWidget
     * @param parent Parent widget pointer
     */
    explicit ModelMetricsWidget(QWidget *parent = nullptr);
    
    /**
     * @brief Destructor
     */
    ~ModelMetricsWidget();

    /**
     * @brief Set binary classification metrics from prediction and true label vectors
     * @param predictions Vector of predicted labels (0 or 1)
     * @param true_labels Vector of true labels (0 or 1)
     * @param model_name Name of the model used for predictions
     * @pre predictions.size() == true_labels.size()
     * @post Binary classification metrics are calculated and displayed
     */
    void setBinaryClassificationMetrics(arma::Row<std::size_t> const & predictions, 
                                       arma::Row<std::size_t> const & true_labels,
                                       std::string const & model_name);

    /**
     * @brief Set binary classification metrics from pre-calculated metrics struct
     * @param metrics Pre-calculated binary classification metrics
     * @param model_name Name of the model used for predictions
     * @post Metrics are displayed in the widget
     */
    void setBinaryClassificationMetrics(BinaryClassificationMetrics const & metrics,
                                       std::string const & model_name);

    /**
     * @brief Clear all metrics and reset widget to default state
     * @post Widget shows "No metrics available" message
     */
    void clearMetrics();

    /**
     * @brief Get current binary classification metrics
     * @return BinaryClassificationMetrics Current metrics (may be invalid if not set)
     */
    [[nodiscard]] BinaryClassificationMetrics getCurrentBinaryMetrics() const;

    /**
     * @brief Check if valid metrics are currently displayed
     * @return bool True if valid metrics are displayed, false otherwise
     */
    [[nodiscard]] bool hasValidMetrics() const;

private slots:
    // Future slots for user interactions can be added here

private:
    /**
     * @brief Calculate binary classification metrics from prediction vectors
     * @param predictions Vector of predicted labels (0 or 1)
     * @param true_labels Vector of true labels (0 or 1)
     * @return BinaryClassificationMetrics Calculated metrics
     * @pre predictions.size() == true_labels.size()
     */
    BinaryClassificationMetrics _calculateBinaryMetrics(arma::Row<std::size_t> const & predictions,
                                                        arma::Row<std::size_t> const & true_labels) const;

    /**
     * @brief Update UI display with binary classification metrics
     * @param metrics Binary classification metrics to display
     * @param model_name Name of the model
     */
    void _updateBinaryClassificationDisplay(BinaryClassificationMetrics const & metrics,
                                           std::string const & model_name);

    /**
     * @brief Format confusion matrix for display
     * @param metrics Binary classification metrics containing confusion matrix data
     * @return QString Formatted confusion matrix string
     */
    QString _formatConfusionMatrix(BinaryClassificationMetrics const & metrics) const;

    /**
     * @brief Set widget to show "no metrics" state
     */
    void _showNoMetricsState();

    Ui::ModelMetricsWidget *ui;                        ///< UI components
    ModelMetricType _current_metric_type;              ///< Currently displayed metric type
    BinaryClassificationMetrics _current_binary_metrics; ///< Current binary classification metrics
};

#endif // MODELMETRICSWIDGET_HPP 