/**
 * @file ClassificationMetrics.cpp
 * @brief Implementation of binary and multi-class classification metric computation
 */

#include "ClassificationMetrics.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace MLCore {

// ============================================================================
// Binary classification
// ============================================================================

BinaryClassificationMetrics computeBinaryMetrics(
        arma::Row<std::size_t> const & predictions,
        arma::Row<std::size_t> const & truth) {
    if (predictions.n_elem != truth.n_elem) {
        throw std::invalid_argument(
                "computeBinaryMetrics: prediction size (" +
                std::to_string(predictions.n_elem) +
                ") != truth size (" +
                std::to_string(truth.n_elem) + ")");
    }

    BinaryClassificationMetrics metrics;

    if (predictions.n_elem == 0) {
        return metrics;// isValid() == false
    }

    for (std::size_t i = 0; i < predictions.n_elem; ++i) {
        std::size_t const pred = predictions[i];
        std::size_t const label = truth[i];

        // Skip non-binary values
        if (pred > 1 || label > 1) {
            continue;
        }

        if (pred == 1 && label == 1) {
            ++metrics.true_positives;
        } else if (pred == 0 && label == 0) {
            ++metrics.true_negatives;
        } else if (pred == 1 && label == 0) {
            ++metrics.false_positives;
        } else {// pred == 0 && label == 1
            ++metrics.false_negatives;
        }
    }

    std::size_t const total = metrics.getTotalPredictions();
    if (total > 0) {
        metrics.accuracy =
                static_cast<double>(metrics.true_positives + metrics.true_negatives) /
                static_cast<double>(total);
    }

    std::size_t const actual_positives = metrics.true_positives + metrics.false_negatives;
    if (actual_positives > 0) {
        metrics.sensitivity =
                static_cast<double>(metrics.true_positives) /
                static_cast<double>(actual_positives);
    }

    std::size_t const actual_negatives = metrics.true_negatives + metrics.false_positives;
    if (actual_negatives > 0) {
        metrics.specificity =
                static_cast<double>(metrics.true_negatives) /
                static_cast<double>(actual_negatives);
    }

    std::size_t const dice_denom =
            2 * metrics.true_positives + metrics.false_positives + metrics.false_negatives;
    if (dice_denom > 0) {
        metrics.dice_score =
                static_cast<double>(2 * metrics.true_positives) /
                static_cast<double>(dice_denom);
    }

    return metrics;
}

// ============================================================================
// Multi-class classification
// ============================================================================

MultiClassMetrics computeMultiClassMetrics(
        arma::Row<std::size_t> const & predictions,
        arma::Row<std::size_t> const & truth,
        std::size_t num_classes) {
    if (predictions.n_elem != truth.n_elem) {
        throw std::invalid_argument(
                "computeMultiClassMetrics: prediction size (" +
                std::to_string(predictions.n_elem) +
                ") != truth size (" +
                std::to_string(truth.n_elem) + ")");
    }

    if (num_classes == 0) {
        throw std::invalid_argument("computeMultiClassMetrics: num_classes must be > 0");
    }

    MultiClassMetrics metrics;
    metrics.num_classes = num_classes;

    if (predictions.n_elem == 0) {
        return metrics;// isValid() == false (confusion_matrix is empty)
    }

    // Build confusion matrix: entry (true_class, predicted_class)
    metrics.confusion_matrix = arma::umat(num_classes, num_classes, arma::fill::zeros);

    std::size_t correct = 0;
    for (std::size_t i = 0; i < predictions.n_elem; ++i) {
        std::size_t const pred = predictions[i];
        std::size_t const label = truth[i];

        if (pred < num_classes && label < num_classes) {
            metrics.confusion_matrix(label, pred) += 1;
            if (pred == label) {
                ++correct;
            }
        }
    }

    metrics.overall_accuracy =
            static_cast<double>(correct) / static_cast<double>(predictions.n_elem);

    // Per-class precision, recall, F1
    metrics.per_class_precision.resize(num_classes, 0.0);
    metrics.per_class_recall.resize(num_classes, 0.0);
    metrics.per_class_f1.resize(num_classes, 0.0);

    for (std::size_t c = 0; c < num_classes; ++c) {
        // TP = confusion_matrix(c, c)
        std::size_t const tp = metrics.confusion_matrix(c, c);

        // FP = sum of column c minus TP (predicted c but not truly c)
        std::size_t const col_sum = arma::accu(metrics.confusion_matrix.col(c));
        std::size_t const fp = col_sum - tp;

        // FN = sum of row c minus TP (truly c but not predicted c)
        std::size_t const row_sum = arma::accu(metrics.confusion_matrix.row(c));
        std::size_t const fn = row_sum - tp;

        double const precision = (tp + fp > 0)
                                         ? static_cast<double>(tp) / static_cast<double>(tp + fp)
                                         : 0.0;

        double const recall = (tp + fn > 0)
                                      ? static_cast<double>(tp) / static_cast<double>(tp + fn)
                                      : 0.0;

        double const f1 = (precision + recall > 0.0)
                                  ? 2.0 * precision * recall / (precision + recall)
                                  : 0.0;

        metrics.per_class_precision[c] = precision;
        metrics.per_class_recall[c] = recall;
        metrics.per_class_f1[c] = f1;
    }

    return metrics;
}

MultiClassMetrics computeMultiClassMetrics(
        arma::Row<std::size_t> const & predictions,
        arma::Row<std::size_t> const & truth) {
    if (predictions.n_elem != truth.n_elem) {
        throw std::invalid_argument(
                "computeMultiClassMetrics: prediction size (" +
                std::to_string(predictions.n_elem) +
                ") != truth size (" +
                std::to_string(truth.n_elem) + ")");
    }

    if (predictions.n_elem == 0) {
        throw std::invalid_argument(
                "computeMultiClassMetrics: cannot infer num_classes from empty vectors");
    }

    std::size_t const max_pred = predictions.max();
    std::size_t const max_truth = truth.max();
    std::size_t const num_classes = std::max(max_pred, max_truth) + 1;

    return computeMultiClassMetrics(predictions, truth, num_classes);
}

// ============================================================================
// Formatting
// ============================================================================

std::string formatBinaryMetrics(BinaryClassificationMetrics const & metrics) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4);
    oss << "Accuracy: " << metrics.accuracy
        << "  Sensitivity: " << metrics.sensitivity
        << "  Specificity: " << metrics.specificity
        << "  Dice/F1: " << metrics.dice_score << '\n';
    oss << "TP: " << metrics.true_positives
        << "  TN: " << metrics.true_negatives
        << "  FP: " << metrics.false_positives
        << "  FN: " << metrics.false_negatives;
    return oss.str();
}

std::string formatMultiClassMetrics(
        MultiClassMetrics const & metrics,
        std::vector<std::string> const & class_names) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4);
    oss << "Overall Accuracy: " << metrics.overall_accuracy << '\n';

    // Per-class metrics table
    oss << "\nClass      Precision  Recall     F1\n";
    oss << std::string(45, '-') << '\n';
    for (std::size_t c = 0; c < metrics.num_classes; ++c) {
        std::string const name = (c < class_names.size()) ? class_names[c] : std::to_string(c);
        oss << std::left << std::setw(10) << name << ' '
            << std::right << std::setw(9) << metrics.per_class_precision[c] << "  "
            << std::setw(9) << metrics.per_class_recall[c] << "  "
            << std::setw(9) << metrics.per_class_f1[c] << '\n';
    }

    // Confusion matrix
    oss << "\nConfusion Matrix (rows=truth, cols=predicted):\n";
    // Header row
    oss << std::setw(10) << "";
    for (std::size_t c = 0; c < metrics.num_classes; ++c) {
        std::string const name = (c < class_names.size()) ? class_names[c] : std::to_string(c);
        oss << std::setw(8) << name;
    }
    oss << '\n';
    // Data rows
    for (std::size_t r = 0; r < metrics.num_classes; ++r) {
        std::string const name = (r < class_names.size()) ? class_names[r] : std::to_string(r);
        oss << std::left << std::setw(10) << name;
        for (std::size_t c = 0; c < metrics.num_classes; ++c) {
            oss << std::right << std::setw(8) << metrics.confusion_matrix(r, c);
        }
        oss << '\n';
    }

    return oss.str();
}

}// namespace MLCore