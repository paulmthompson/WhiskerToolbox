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
    int num_trees = 10;              ///< Number of trees in the ensemble
    int minimum_leaf_size = 1;       ///< Minimum number of samples in a leaf node
    double minimum_gain_split = 1e-7;///< Minimum gain to split a node
    int maximum_depth = 0;           ///< Maximum tree depth (0 = unlimited)
};

/**
 * @brief Parameters for Naive Bayes classifier
 *
 * Maps to mlpack::NaiveBayesClassifier hyperparameters.
 */
struct NaiveBayesParameters : public MLModelParametersBase {
    double epsilon = 1e-9;///< Variance smoothing parameter
};

/**
 * @brief Parameters for Logistic Regression classifier
 *
 * Maps to mlpack::LogisticRegression hyperparameters.
 * Naturally produces probability estimates.
 */
struct LogisticRegressionParameters : public MLModelParametersBase {
    double lambda = 0.0;               ///< L2 regularization parameter
    double step_size = 0.01;           ///< Optimizer step size
    std::size_t max_iterations = 10000;///< Maximum optimizer iterations (0 = unlimited)
};

/**
 * @brief Parameters for Softmax Regression (multi-class logistic regression)
 *
 * Maps to mlpack::SoftmaxRegression hyperparameters.
 * Generalizes binary logistic regression to C ≥ 2 classes.
 * Produces calibrated per-class probability estimates via softmax.
 */
struct SoftmaxRegressionParameters : public MLModelParametersBase {
    double lambda = 0.0001;            ///< L2 regularization parameter
    std::size_t max_iterations = 10000;///< Maximum L-BFGS iterations (0 = unlimited)
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
    std::size_t k = 3;                ///< Number of clusters
    std::size_t max_iterations = 1000;///< Maximum iterations (0 = unlimited)
    std::optional<std::size_t> seed;  ///< Random seed for reproducibility
};

/**
 * @brief Parameters for DBSCAN clustering
 *
 * Maps to mlpack::DBSCAN hyperparameters.
 */
struct DBSCANParameters : public MLModelParametersBase {
    double epsilon = 1.0;      ///< Neighborhood radius
    std::size_t min_points = 5;///< Minimum points to form a dense region
};

/**
 * @brief Parameters for Gaussian Mixture Model clustering
 *
 * Maps to mlpack::GMM hyperparameters.
 */
struct GMMParameters : public MLModelParametersBase {
    std::size_t k = 3;               ///< Number of Gaussian components
    std::size_t max_iterations = 300;///< Maximum EM iterations
    std::optional<std::size_t> seed; ///< Random seed for reproducibility
};

// ============================================================================
// Dimensionality reduction parameters
// ============================================================================

/**
 * @brief Parameters for PCA dimensionality reduction
 *
 * Wraps mlpack::PCA hyperparameters.
 */
struct PCAParameters : public MLModelParametersBase {
    std::size_t n_components = 2;///< Number of principal components to retain
};

/**
 * @brief Parameters for t-SNE dimensionality reduction
 *
 * Wraps tapkee's t-SNE implementation. t-SNE is a non-linear, stochastic
 * method suited for 2D/3D visualization of high-dimensional data.
 *
 * @note t-SNE does not support projecting new data (transform is unsupported).
 */
struct TSNEParameters : public MLModelParametersBase {
    std::size_t n_components = 2;///< Number of output dimensions (usually 2 or 3)
    double perplexity = 30.0;    ///< Perplexity (effective number of neighbors; typical range 5–50)
    double theta = 0.5;          ///< Barnes-Hut approximation angle (0 = exact, >0 = faster)
};

/**
 * @brief Parameters for Robust PCA (ROSL) dimensionality reduction
 *
 * Wraps the ROSL (Robust Online Subspace Learning) algorithm which
 * decomposes a matrix into a low-rank component and sparse errors:
 * X = A + E, where A = D * B is the low-rank approximation.
 *
 * Robust PCA supports projecting new data via the learned dictionary.
 */
struct RobustPCAParameters : public MLModelParametersBase {
    std::size_t n_components = 2;///< Number of output dimensions (max rank for ROSL)
    double lambda = 0.0;         ///< Regularization (0 = auto-compute from data)
    double tol = 1e-5;           ///< Convergence tolerance
    std::size_t max_iter = 100;  ///< Maximum ALM iterations
};


// ============================================================================
// Supervised dimensionality reduction parameters
// ============================================================================

/**
 * @brief Parameters for Logit Projection (supervised dimensionality reduction)
 *
 * LogitProjectionOperation trains a softmax regression classifier on labeled
 * data and extracts the pre-softmax logit activations as a C-dimensional
 * discriminative projection. Uses L2-regularized L-BFGS optimization.
 */
struct LogitProjectionParameters : public MLModelParametersBase {
    double lambda = 0.0001;            ///< L2 regularization strength (same as SoftmaxRegressionParameters)
    std::size_t max_iterations = 10000;///< Maximum L-BFGS optimizer iterations
    bool scale_features = false;       ///< Standardize features (zero mean, unit variance) before fitting
};

/**
 * @brief Parameters for Supervised PCA dimensionality reduction
 *
 * Supervised PCA fits PCA on the labeled subset of observations only,
 * so that the principal components capture the variance structure of the
 * labeled data. All observations are then projected into that space.
 */
struct SupervisedPCAParameters : public MLModelParametersBase {
    std::size_t n_components = 2;///< Number of principal components to retain
    bool scale = true;           ///< Standardize features (zero mean, unit variance) before PCA
};

/**
 * @brief Parameters for Supervised Robust PCA dimensionality reduction
 *
 * Supervised Robust PCA fits ROSL (Robust Online Subspace Learning) on the
 * labeled subset of observations only, decomposing into a low-rank component
 * and sparse errors. The principal components capture the variance structure
 * of the labeled data while being robust to outliers.
 */
struct SupervisedRobustPCAParameters : public MLModelParametersBase {
    std::size_t n_components = 2;///< Number of output dimensions
    double lambda = 0.0;         ///< Regularization (0 = auto-compute from data)
    double tol = 1e-5;           ///< Convergence tolerance
    std::size_t max_iter = 100;  ///< Maximum ALM iterations
};

// ============================================================================
// Sequence model parameters
// ============================================================================

/**
 * @brief Parameters for Hidden Markov Model (Gaussian emissions)
 *
 * Maps to mlpack::HMM<mlpack::GaussianDistribution<>> hyperparameters.
 * Supervised training uses labeled sequences to estimate transition and
 * emission parameters directly.
 *
 * When `use_diagonal_covariance` is true, uses
 * mlpack::DiagonalGaussianDistribution instead of the full covariance
 * variant. Diagonal covariance reduces parameters per state from
 * M(M+1)/2 to M, improving numerical stability with limited training
 * data and reducing emission evaluation cost from O(M^2) to O(M).
 */
struct HMMParameters : public MLModelParametersBase {
    std::size_t num_states = 2;          ///< Number of hidden states
    double tolerance = 1e-5;             ///< Convergence tolerance for training
    bool use_diagonal_covariance = false;///< Use diagonal covariance emissions
    bool use_gmm_emissions = false;      ///< Use Gaussian Mixture Model emissions per state
    std::size_t num_gaussians = 3;       ///< Number of Gaussian components per state (when use_gmm_emissions is true)
};

}// namespace MLCore

#endif// MLCORE_MLMODELPARAMETERS_HPP
