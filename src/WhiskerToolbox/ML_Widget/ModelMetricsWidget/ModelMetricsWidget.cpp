#include "ModelMetricsWidget.hpp"
#include "ui_ModelMetricsWidget.h"

#include <QString>
#include <QStackedWidget>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

ModelMetricsWidget::ModelMetricsWidget(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::ModelMetricsWidget),
      _current_metric_type(ModelMetricType::BinaryClassification) {
    ui->setupUi(this);
    
    // Initialize to "no metrics" state
    _showNoMetricsState();
}

ModelMetricsWidget::~ModelMetricsWidget() {
    delete ui;
}

void ModelMetricsWidget::setBinaryClassificationMetrics(arma::Row<std::size_t> const & predictions, 
                                                       arma::Row<std::size_t> const & true_labels,
                                                       std::string const & model_name) {
    if (predictions.n_elem != true_labels.n_elem) {
        std::cerr << "Error: Prediction and true label vectors must have the same size." << std::endl;
        _showNoMetricsState();
        return;
    }
    
    if (predictions.n_elem == 0) {
        std::cerr << "Warning: Empty prediction vectors provided." << std::endl;
        _showNoMetricsState();
        return;
    }
    
    // Calculate metrics from the prediction vectors
    BinaryClassificationMetrics metrics = _calculateBinaryMetrics(predictions, true_labels);
    setBinaryClassificationMetrics(metrics, model_name);
}

void ModelMetricsWidget::setBinaryClassificationMetrics(BinaryClassificationMetrics const & metrics,
                                                       std::string const & model_name) {
    if (!metrics.isValid()) {
        std::cerr << "Warning: Invalid metrics provided (no predictions)." << std::endl;
        _showNoMetricsState();
        return;
    }
    
    _current_binary_metrics = metrics;
    _current_metric_type = ModelMetricType::BinaryClassification;
    _updateBinaryClassificationDisplay(metrics, model_name);
}

void ModelMetricsWidget::clearMetrics() {
    _current_binary_metrics = BinaryClassificationMetrics{};
    _showNoMetricsState();
}

BinaryClassificationMetrics ModelMetricsWidget::getCurrentBinaryMetrics() const {
    return _current_binary_metrics;
}

bool ModelMetricsWidget::hasValidMetrics() const {
    return _current_binary_metrics.isValid();
}

BinaryClassificationMetrics ModelMetricsWidget::_calculateBinaryMetrics(arma::Row<std::size_t> const & predictions,
                                                                        arma::Row<std::size_t> const & true_labels) const {
    BinaryClassificationMetrics metrics;
    
    // Count confusion matrix components
    for (std::size_t i = 0; i < predictions.n_elem; ++i) {
        std::size_t pred = predictions[i];
        std::size_t truth = true_labels[i];
        
        // Ensure binary classification (0 or 1)
        if (pred > 1 || truth > 1) {
            std::cerr << "Warning: Non-binary values detected in classification. "
                      << "Pred: " << pred << ", Truth: " << truth << " at index " << i << std::endl;
        }
        
        if (pred == 1 && truth == 1) {
            metrics.true_positives++;
        } else if (pred == 0 && truth == 0) {
            metrics.true_negatives++;
        } else if (pred == 1 && truth == 0) {
            metrics.false_positives++;
        } else if (pred == 0 && truth == 1) {
            metrics.false_negatives++;
        }
    }
    
    // Calculate metrics with proper handling of division by zero
    std::size_t total = metrics.getTotalPredictions();
    if (total > 0) {
        metrics.accuracy = static_cast<double>(metrics.true_positives + metrics.true_negatives) / static_cast<double>(total);
    }
    
    std::size_t actual_positives = metrics.true_positives + metrics.false_negatives;
    if (actual_positives > 0) {
        metrics.sensitivity = static_cast<double>(metrics.true_positives) / static_cast<double>(actual_positives);
    }
    
    std::size_t actual_negatives = metrics.true_negatives + metrics.false_positives;
    if (actual_negatives > 0) {
        metrics.specificity = static_cast<double>(metrics.true_negatives) / static_cast<double>(actual_negatives);
    }
    
    std::size_t predicted_and_actual_positives = 2 * metrics.true_positives + metrics.false_positives + metrics.false_negatives;
    if (predicted_and_actual_positives > 0) {
        metrics.dice_score = static_cast<double>(2 * metrics.true_positives) / static_cast<double>(predicted_and_actual_positives);
    }
    
    return metrics;
}

void ModelMetricsWidget::_updateBinaryClassificationDisplay(BinaryClassificationMetrics const & metrics,
                                                           std::string const & model_name) {
    // Set model type label
    ui->modelTypeLabel->setText(QString("Model Type: %1 (Binary Classification)").arg(QString::fromStdString(model_name)));
    
    // Update metric values with proper formatting (2 decimal places, percentage)
    ui->accuracyValueLabel->setText(QString("%1%").arg(metrics.accuracy * 100.0, 0, 'f', 2));
    ui->sensitivityValueLabel->setText(QString("%1%").arg(metrics.sensitivity * 100.0, 0, 'f', 2));
    ui->specificityValueLabel->setText(QString("%1%").arg(metrics.specificity * 100.0, 0, 'f', 2));
    ui->diceScoreValueLabel->setText(QString("%1%").arg(metrics.dice_score * 100.0, 0, 'f', 2));
    
    // Update confusion matrix display
    QString confusion_matrix = _formatConfusionMatrix(metrics);
    ui->confusionMatrixDisplayLabel->setText(confusion_matrix);
    
    // Show the binary classification page
    ui->metricsStackedWidget->setCurrentWidget(ui->binaryClassificationPage);
    
    std::cout << "Updated binary classification metrics display for model: " << model_name << std::endl;
    std::cout << "Accuracy: " << (metrics.accuracy * 100.0) << "%, "
              << "Sensitivity: " << (metrics.sensitivity * 100.0) << "%, "
              << "Specificity: " << (metrics.specificity * 100.0) << "%, "
              << "Dice Score: " << (metrics.dice_score * 100.0) << "%" << std::endl;
}

QString ModelMetricsWidget::_formatConfusionMatrix(BinaryClassificationMetrics const & metrics) const {
    std::ostringstream oss;
    
    // Calculate field width for proper alignment
    std::size_t max_value = std::max({metrics.true_positives, metrics.true_negatives, 
                                     metrics.false_positives, metrics.false_negatives});
    int field_width = std::to_string(max_value).length() + 1;
    field_width = std::max(field_width, 4); // Minimum width for readability
    
    oss << "           Predicted\n";
    oss << "         │   0  │   1  │\n";
    oss << "       ──┼──────┼──────┤\n";
    oss << "Actual 0 │" << std::setw(field_width) << metrics.true_negatives 
        << " │" << std::setw(field_width) << metrics.false_positives << " │\n";
    oss << "       1 │" << std::setw(field_width) << metrics.false_negatives 
        << " │" << std::setw(field_width) << metrics.true_positives << " │\n";
    oss << "\n";
    oss << "Total samples: " << metrics.getTotalPredictions();
    
    return QString::fromStdString(oss.str());
}

void ModelMetricsWidget::_showNoMetricsState() {
    ui->modelTypeLabel->setText("Model Type: Not Set");
    ui->metricsStackedWidget->setCurrentWidget(ui->noMetricsPage);
    std::cout << "ModelMetricsWidget: Showing 'no metrics' state" << std::endl;
} 