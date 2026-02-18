#ifndef MLCORE_MLMODELOPERATION_HPP
#define MLCORE_MLMODELOPERATION_HPP

/**
 * @file MLModelOperation.hpp
 * @brief Abstract interface for all ML model operations (supervised & unsupervised)
 *
 * This is the evolved model interface for the MLCore library, replacing the
 * legacy `MLModelOperation` in `src/WhiskerToolbox/ML_Widget/MLModelOperation.hpp`.
 *
 * Key extensions over the legacy interface:
 * - **MLTaskType**: each operation declares what task it performs, enabling
 *   the registry to filter by task type
 * - **Unsupervised support**: `fit()` and `assignClusters()` for clustering
 *   algorithms (K-Means, DBSCAN, GMM)
 * - **Persistence**: `save()` / `load()` for serializing trained models
 * - **Query methods**: `isTrained()`, `numClasses()`, `numFeatures()` for
 *   introspection after training
 * - **Probability output**: `predictProbabilities()` returns a per-class
 *   probability matrix (num_classes × num_observations)
 *
 * All methods operate on armadillo matrices in mlpack convention:
 * - Feature matrices are (features × observations) — each column is one sample
 * - Label rows are (1 × observations)
 * - Probability matrices are (num_classes × observations)
 *
 * Concrete implementations (RandomForestOperation, KMeansOperation, etc.)
 * live in `models/supervised/` and `models/unsupervised/` subdirectories.
 *
 * @see ml_library_roadmap.md §3.4.1
 */

#include "MLModelParameters.hpp"
#include "MLTaskType.hpp"

#include <armadillo>

#include <cstddef>
#include <iostream>
#include <memory>
#include <string>

namespace MLCore {

/**
 * @brief Abstract base for all ML model operations
 *
 * A model operation encapsulates a single ML algorithm. It can be trained
 * (supervised) or fit (unsupervised), then used for prediction or cluster
 * assignment. Each operation declares its task type so the registry can
 * categorize it.
 *
 * ## Supervised workflow
 * ```
 * op->train(features, labels, params);
 * op->predict(features, predictions);
 * // Optional:
 * op->predictProbabilities(features, probabilities);
 * ```
 *
 * ## Unsupervised workflow
 * ```
 * op->fit(features, params);
 * op->assignClusters(features, assignments);
 * ```
 *
 * ## Lifecycle
 * 1. Construct via factory (MLModelRegistry::create)
 * 2. Train/fit with parameters
 * 3. Query `isTrained()` to confirm success
 * 4. Predict/assign on new data
 * 5. Optionally save/load for persistence
 */
class MLModelOperation {
public:
    virtual ~MLModelOperation() = default;

    // ========================================================================
    // Identity & metadata
    // ========================================================================

    /**
     * @brief Unique human-readable name for this model (e.g., "Random Forest")
     *
     * Used as the key in MLModelRegistry and displayed in the UI.
     */
    [[nodiscard]] virtual std::string getName() const = 0;

    /**
     * @brief The ML task type this operation supports
     *
     * Enables the registry to filter models by task (classification vs clustering).
     */
    [[nodiscard]] virtual MLTaskType getTaskType() const = 0;

    /**
     * @brief Create a default parameter set for this model
     *
     * Returns a new instance with algorithm-specific defaults.
     * The caller owns the returned pointer.
     */
    [[nodiscard]] virtual std::unique_ptr<MLModelParametersBase> getDefaultParameters() const = 0;

    // ========================================================================
    // Supervised training & prediction
    // ========================================================================

    /**
     * @brief Train the model on labeled data
     *
     * @param features  Feature matrix (features × observations). Each column
     *                  is one observation with `features.n_rows` feature values.
     * @param labels    Label row vector (1 × observations). Each element is
     *                  a class index in [0, num_classes).
     * @param params    Algorithm-specific parameters. May be nullptr to use
     *                  defaults. Must be castable to the concrete parameter
     *                  type expected by this model.
     * @return true if training succeeded
     *
     * After successful training, `isTrained()` returns true, and `numClasses()`
     * and `numFeatures()` reflect the training data dimensions.
     *
     * The default implementation returns false (unsupervised models).
     */
    virtual bool train(
        arma::mat const & features,
        arma::Row<std::size_t> const & labels,
        MLModelParametersBase const * params)
    {
        static_cast<void>(features);
        static_cast<void>(labels);
        static_cast<void>(params);
        return false;
    }

    /**
     * @brief Predict class labels for new observations
     *
     * @param features     Feature matrix (features × observations)
     * @param predictions  [out] Predicted label row vector (1 × observations)
     * @return true if prediction succeeded
     *
     * Requires `isTrained() == true`. The number of feature rows must match
     * the training data dimensionality (`numFeatures()`).
     *
     * The default implementation returns false (unsupervised models).
     */
    virtual bool predict(
        arma::mat const & features,
        arma::Row<std::size_t> & predictions)
    {
        static_cast<void>(features);
        static_cast<void>(predictions);
        return false;
    }

    /**
     * @brief Predict class probabilities for new observations
     *
     * @param features       Feature matrix (features × observations)
     * @param probabilities  [out] Probability matrix (num_classes × observations).
     *                       Each column sums to 1.0.
     * @return true if probability estimation is supported and succeeded
     *
     * Not all models support probability output. The default returns false.
     * Models that do support it (e.g., Logistic Regression, Random Forest)
     * override this method.
     */
    virtual bool predictProbabilities(
        arma::mat const & features,
        arma::mat & probabilities)
    {
        static_cast<void>(features);
        static_cast<void>(probabilities);
        return false;
    }

    // ========================================================================
    // Unsupervised fitting & cluster assignment
    // ========================================================================

    /**
     * @brief Fit the model to unlabeled data (unsupervised)
     *
     * @param features  Feature matrix (features × observations)
     * @param params    Algorithm-specific parameters (K, epsilon, etc.)
     * @return true if fitting succeeded
     *
     * After successful fitting, `isTrained()` returns true (the model is
     * ready for `assignClusters()`), and `numClasses()` returns the number
     * of clusters found/requested.
     *
     * The default implementation returns false (supervised models).
     */
    virtual bool fit(
        arma::mat const & features,
        MLModelParametersBase const * params)
    {
        static_cast<void>(features);
        static_cast<void>(params);
        return false;
    }

    /**
     * @brief Assign cluster labels to observations
     *
     * @param features     Feature matrix (features × observations)
     * @param assignments  [out] Cluster assignment row vector (1 × observations).
     *                     Each element is a cluster index in [0, K).
     * @return true if assignment succeeded
     *
     * Requires `isTrained() == true` (model has been fit).
     *
     * The default implementation returns false (supervised models).
     */
    virtual bool assignClusters(
        arma::mat const & features,
        arma::Row<std::size_t> & assignments)
    {
        static_cast<void>(features);
        static_cast<void>(assignments);
        return false;
    }

    // ========================================================================
    // Serialization
    // ========================================================================

    /**
     * @brief Save the trained model to a stream
     *
     * @param out  Output stream (binary or text, model-dependent)
     * @return true if save succeeded
     *
     * The default implementation returns false (no persistence support).
     */
    virtual bool save(std::ostream & out) const
    {
        static_cast<void>(out);
        return false;
    }

    /**
     * @brief Load a previously saved model from a stream
     *
     * @param in  Input stream
     * @return true if load succeeded
     *
     * After successful loading, `isTrained()` should return true.
     * The default implementation returns false (no persistence support).
     */
    virtual bool load(std::istream & in)
    {
        static_cast<void>(in);
        return false;
    }

    // ========================================================================
    // Query / introspection
    // ========================================================================

    /**
     * @brief Whether the model has been trained (supervised) or fit (unsupervised)
     *
     * Returns true after a successful `train()` or `fit()` call, or after
     * a successful `load()`.
     */
    [[nodiscard]] virtual bool isTrained() const = 0;

    /**
     * @brief Number of classes (supervised) or clusters (unsupervised)
     *
     * Returns 0 if the model has not been trained/fit.
     * For binary classification, returns 2.
     */
    [[nodiscard]] virtual std::size_t numClasses() const { return 0; }

    /**
     * @brief Number of features the model was trained on
     *
     * Returns 0 if the model has not been trained/fit.
     * Used for validating that prediction data has the correct dimensionality.
     */
    [[nodiscard]] virtual std::size_t numFeatures() const { return 0; }
};

} // namespace MLCore

#endif // MLCORE_MLMODELOPERATION_HPP
