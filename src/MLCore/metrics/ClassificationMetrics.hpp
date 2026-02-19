#ifndef MLCORE_CLASSIFICATIONMETRICS_HPP
#define MLCORE_CLASSIFICATIONMETRICS_HPP

/**
 * @file ClassificationMetrics.hpp
 * @brief Classification metric computation for binary and multi-class problems
 *
 * Provides pure functions (no Qt or UI dependency) for computing standard
 * classification metrics from prediction and ground-truth label vectors.
 *
 * Binary metrics include accuracy, sensitivity (recall), specificity,
 * dice score (F1), and the full confusion matrix counts.
 *
 * Multi-class metrics include overall accuracy, per-class precision/recall/F1,
 * and a full confusion matrix.
 *
 * Extracted from the legacy ModelMetricsWidget to make metrics testable and
 * reusable without GUI dependencies.
 *
 * @see ml_library_roadmap.md §3.4.6
 */

#include <armadillo>

#include <cstddef>
#include <string>
#include <vector>

namespace MLCore {

// ============================================================================
// Binary classification metrics
// ============================================================================

/**
 * @brief Metrics for a binary (two-class) classification problem
 *
 * All rate metrics (accuracy, sensitivity, specificity, dice_score) are in [0, 1].
 * When a denominator is zero (e.g., no actual positives for sensitivity), the
 * corresponding metric is 0.0.
 */
struct BinaryClassificationMetrics {
    double accuracy    = 0.0;  ///< (TP + TN) / Total
    double sensitivity = 0.0;  ///< TP / (TP + FN)  — also called recall
    double specificity = 0.0;  ///< TN / (TN + FP)
    double dice_score  = 0.0;  ///< 2·TP / (2·TP + FP + FN) — also called F1

    std::size_t true_positives  = 0;
    std::size_t true_negatives  = 0;
    std::size_t false_positives = 0;
    std::size_t false_negatives = 0;

    /**
     * @brief Check if any predictions were counted
     * @return true if at least one prediction contributed to the confusion matrix
     */
    [[nodiscard]] bool isValid() const {
        return (true_positives + true_negatives + false_positives + false_negatives) > 0;
    }

    /**
     * @brief Total number of predictions (TP + TN + FP + FN)
     */
    [[nodiscard]] std::size_t getTotalPredictions() const {
        return true_positives + true_negatives + false_positives + false_negatives;
    }
};

// ============================================================================
// Multi-class classification metrics
// ============================================================================

/**
 * @brief Metrics for a multi-class classification problem
 *
 * The confusion matrix is stored as an arma::umat of shape (num_classes × num_classes).
 * Entry (i, j) is the count of observations with true class i that were predicted as class j.
 *
 * Per-class precision, recall, and F1 are vectors of size num_classes.
 * When a denominator is zero, the corresponding metric is 0.0.
 */
struct MultiClassMetrics {
    double overall_accuracy = 0.0;           ///< Fraction of correct predictions
    arma::umat confusion_matrix;             ///< Shape: (num_classes × num_classes), entry (true, pred)
    std::vector<double> per_class_precision;  ///< precision[i] = TP_i / (TP_i + FP_i)
    std::vector<double> per_class_recall;     ///< recall[i]    = TP_i / (TP_i + FN_i)
    std::vector<double> per_class_f1;         ///< f1[i]        = 2·P·R / (P + R)
    std::size_t num_classes = 0;             ///< Number of classes

    /**
     * @brief Check if any predictions were counted
     */
    [[nodiscard]] bool isValid() const {
        return num_classes > 0 && confusion_matrix.n_elem > 0;
    }
};

// ============================================================================
// Computation functions
// ============================================================================

/**
 * @brief Compute binary classification metrics from predictions and ground truth
 *
 * Labels must be binary (0 or 1). Non-binary values are ignored with a warning
 * count in the returned struct. Label 1 is treated as positive, 0 as negative.
 *
 * @param predictions Predicted labels (0 or 1), one per observation
 * @param truth       Ground truth labels (0 or 1), one per observation
 * @return BinaryClassificationMetrics with all fields populated
 * @throws std::invalid_argument if vectors have different sizes
 *
 * @note If both vectors are empty, returns a default (invalid) metrics struct.
 */
[[nodiscard]] BinaryClassificationMetrics computeBinaryMetrics(
    arma::Row<std::size_t> const & predictions,
    arma::Row<std::size_t> const & truth);

/**
 * @brief Compute multi-class classification metrics
 *
 * @param predictions Predicted class labels, one per observation
 * @param truth       Ground truth class labels, one per observation
 * @param num_classes Total number of classes (labels are expected in [0, num_classes-1])
 * @return MultiClassMetrics with confusion matrix and per-class metrics
 * @throws std::invalid_argument if vectors have different sizes or num_classes is zero
 *
 * @note If both vectors are empty, returns a default (invalid) metrics struct.
 */
[[nodiscard]] MultiClassMetrics computeMultiClassMetrics(
    arma::Row<std::size_t> const & predictions,
    arma::Row<std::size_t> const & truth,
    std::size_t num_classes);

/**
 * @brief Infer the number of classes from label vectors and compute multi-class metrics
 *
 * Convenience overload that determines num_classes as max(predictions, truth) + 1.
 *
 * @param predictions Predicted class labels
 * @param truth Ground truth class labels
 * @return MultiClassMetrics
 * @throws std::invalid_argument if vectors have different sizes
 */
[[nodiscard]] MultiClassMetrics computeMultiClassMetrics(
    arma::Row<std::size_t> const & predictions,
    arma::Row<std::size_t> const & truth);

/**
 * @brief Format a BinaryClassificationMetrics struct as a human-readable string
 *
 * Useful for logging and debugging. Example output:
 * @code
 * Accuracy: 0.9400  Sensitivity: 0.9180  Specificity: 0.9540  Dice/F1: 0.9200
 * TP: 45  TN: 50  FP: 3  FN: 3
 * @endcode
 *
 * @param metrics The metrics to format
 * @return Formatted string
 */
[[nodiscard]] std::string formatBinaryMetrics(BinaryClassificationMetrics const & metrics);

/**
 * @brief Format a MultiClassMetrics struct as a human-readable string
 *
 * @param metrics The metrics to format
 * @param class_names Optional class names for labeling rows/columns
 * @return Formatted string including confusion matrix and per-class metrics
 */
[[nodiscard]] std::string formatMultiClassMetrics(
    MultiClassMetrics const & metrics,
    std::vector<std::string> const & class_names = {});

} // namespace MLCore

#endif // MLCORE_CLASSIFICATIONMETRICS_HPP