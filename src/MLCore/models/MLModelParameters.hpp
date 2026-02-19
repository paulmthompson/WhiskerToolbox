#ifndef MLCORE_MLMODELPARAMETERS_HPP
#define MLCORE_MLMODELPARAMETERS_HPP

/**
 * @file MLModelParameters.hpp
 * @brief Base class and concrete parameter types for ML model operations
 *
 * Every MLModelOperation has associated parameters that control training behavior.
 * The base class provides a uniform interface; concrete subclasses add
 * algorithm-specific knobs (number of trees, epsilon, K, etc.).
 *
 * Parameter types in this file are for the MLCore layer (no Qt dependency).
 * UI-specific parameter widgets live in MLCore_Widget.
 *
 * Evolved from the legacy `MLModelParametersBase` / `RandomForestParameters` /
 * `NaiveBayesParameters` in `src/WhiskerToolbox/ML_Widget/MLModelParameters.hpp`.
 * The legacy types remain untouched until Phase 6 (deprecation).
 *
 * @see ml_library_roadmap.md §3.4.1
 */

#include <cstddef>
#include <optional>

namespace MLCore {

// ============================================================================
// Base parameters
// ============================================================================

/**
 * @brief Abstract base for all model-specific parameter sets
 *
 * Subclass this for each model type (Random Forest, K-Means, etc.).
 * The MLModelOperation::getDefaultParameters() factory returns a concrete
 * instance wrapped in `std::unique_ptr<MLModelParametersBase>`.
 */
struct MLModelParametersBase {
    virtual ~MLModelParametersBase() = default;
};

// ============================================================================
// Supervised classification parameters
// ============================================================================

/**
 * @brief Parameters for Random Forest classifier
 *
 * Maps to mlpack::RandomForest hyperparameters.
 */
struct RandomForestParameters : public MLModelParametersBase {
    int num_trees         = 10;     ///< Number of trees in the ensemble
    int minimum_leaf_size = 1;      ///< Minimum number of samples in a leaf node
    double minimum_gain_split = 1e-7; ///< Minimum gain to split a node
    int maximum_depth     = 0;      ///< Maximum tree depth (0 = unlimited)
};

/**
 * @brief Parameters for Naive Bayes classifier
 *
 * Maps to mlpack::NaiveBayesClassifier hyperparameters.
 */
struct NaiveBayesParameters : public MLModelParametersBase {
    double epsilon = 1e-9;  ///< Variance smoothing parameter
};

/**
 * @brief Parameters for Logistic Regression classifier
 *
 * Maps to mlpack::LogisticRegression hyperparameters.
 * Naturally produces probability estimates.
 */
struct LogisticRegressionParameters : public MLModelParametersBase {
    double lambda       = 0.0;     ///< L2 regularization parameter
    double step_size    = 0.01;    ///< Optimizer step size
    std::size_t max_iterations = 10000; ///< Maximum optimizer iterations (0 = unlimited)
};

// ============================================================================
// Unsupervised clustering parameters
// ============================================================================

/**
 * @brief Parameters for K-Means clustering
 *
 * Maps to mlpack::KMeans hyperparameters.
 */
struct KMeansParameters : public MLModelParametersBase {
    std::size_t k              = 3;      ///< Number of clusters
    std::size_t max_iterations = 1000;   ///< Maximum iterations (0 = unlimited)
    std::optional<std::size_t> seed;     ///< Random seed for reproducibility
};

/**
 * @brief Parameters for DBSCAN clustering
 *
 * Maps to mlpack::DBSCAN hyperparameters.
 */
struct DBSCANParameters : public MLModelParametersBase {
    double epsilon     = 1.0;   ///< Neighborhood radius
    std::size_t min_points = 5; ///< Minimum points to form a dense region
};

/**
 * @brief Parameters for Gaussian Mixture Model clustering
 *
 * Maps to mlpack::GMM hyperparameters.
 */
struct GMMParameters : public MLModelParametersBase {
    std::size_t k              = 3;     ///< Number of Gaussian components
    std::size_t max_iterations = 300;   ///< Maximum EM iterations
    std::optional<std::size_t> seed;    ///< Random seed for reproducibility
};

} // namespace MLCore

#endif // MLCORE_MLMODELPARAMETERS_HPP
