/**
 * @file SupervisedPCAOperation.hpp
 * @brief Supervised PCA fits PCA on labeled subset, projects all data
 *
 * Standard PCA finds components that maximize variance across the entire
 * dataset. Supervised PCA fits only on the subset of observations that
 * carry a label, so the principal components capture the variance structure
 * of the labeled data. All observations (labeled + unlabeled) are then
 * projected into that space.
 *
 * This is useful when the labeled data represents a specific phenomenon
 * of interest (e.g., whisker contact), and the user wants the top
 * components to reflect variance within that phenomenon rather than
 * variance across the full recording.
 *
 * Inherits from MLSupervisedDimReductionOperation so it integrates with
 * the SupervisedDimReductionPipeline and scatter-plot visualization.
 */

#ifndef MLCORE_SUPERVISEDPCAOPERATION_HPP
#define MLCORE_SUPERVISEDPCAOPERATION_HPP

#include "MLCore/models/MLModelParameters.hpp"
#include "MLCore/models/MLSupervisedDimReductionOperation.hpp"
#include "MLCore/models/MLTaskType.hpp"

#include <armadillo>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace MLCore {

/**
 * @brief Supervised PCA dimensionality reduction
 *
 * Fits PCA on the labeled subset of observations, then projects all data
 * (labeled and unlabeled) into the resulting principal-component space.
 *
 * ## Usage
 * ```cpp
 * SupervisedPCAOperation spca;
 * SupervisedPCAParameters params;
 * params.n_components = 10;
 *
 * arma::mat result;
 * spca.fitTransform(features, labels, &params, result);
 * // result is (n_components × N) for ALL observations
 *
 * // Re-project new data
 * arma::mat new_result;
 * spca.transform(new_features, new_result);
 *
 * // Explained variance ratio (of the labeled subset)
 * auto ratios = spca.explainedVarianceRatio();
 * ```
 *
 * @note Output convention: (n_components × N), each column is one observation.
 * Unlike LogitProjectionOperation where output dims = num_classes, here the
 * user controls n_components directly.
 */
class SupervisedPCAOperation : public MLSupervisedDimReductionOperation {
public:
    SupervisedPCAOperation();
    ~SupervisedPCAOperation() override;

    // Non-copyable (owns fitted state)
    SupervisedPCAOperation(SupervisedPCAOperation const &) = delete;
    SupervisedPCAOperation & operator=(SupervisedPCAOperation const &) = delete;

    // Movable
    SupervisedPCAOperation(SupervisedPCAOperation &&) noexcept;
    SupervisedPCAOperation & operator=(SupervisedPCAOperation &&) noexcept;

    // ========================================================================
    // Identity & metadata
    // ========================================================================

    [[nodiscard]] std::string getName() const override;
    [[nodiscard]] MLTaskType getTaskType() const override;
    [[nodiscard]] std::unique_ptr<MLModelParametersBase> getDefaultParameters() const override;

    // ========================================================================
    // Supervised dimensionality reduction
    // ========================================================================

    /**
     * @brief Fit PCA on labeled observations and project ALL data
     *
     * Only the observations whose indices appear with any label in [0, C)
     * are used to compute the covariance matrix and eigenvectors. The
     * resulting projection is then applied to ALL columns in features.
     * Internally centers on the labeled-subset mean before eigendecomposition.
     * Feature scaling (z-score) is intentionally NOT performed here — it is
     * the caller's responsibility to pre-scale via FeatureConverter if desired.
     *
     * @param features  (D × N) feature matrix in mlpack convention
     * @param labels    (N,) label row with class indices in [0, C)
     * @param params    SupervisedPCAParameters or nullptr for defaults
     * @param result    (n_components × N) output — all observations projected
     * @return true on success, false on validation failure or eigendecomposition error
     *
     * @pre features must not be empty (enforcement: runtime_check)
     * @pre features must have at least 2 columns (observations) (enforcement: runtime_check)
     * @pre features must not contain NaN or Inf — covariance computation and
     *      eigendecomposition produce garbage otherwise (enforcement: none) [IMPORTANT]
     * @pre labels must not be empty (enforcement: runtime_check)
     * @pre labels.n_elem must equal features.n_cols (enforcement: runtime_check)
     * @pre labels must contain at least 2 distinct class values (enforcement: runtime_check)
     * @pre label values must be class indices and must not overflow std::size_t
     *      (enforcement: none) [LOW]
     * @pre params, if non-null, must be dynamic_castable to SupervisedPCAParameters
     *      (enforcement: runtime_check — falls back to defaults if cast fails)
     * @pre SupervisedPCAParameters::n_components must be > 0 (enforcement: runtime_check)
     * @pre SupervisedPCAParameters::n_components must be <= features.n_rows (enforcement: runtime_check)
     *
     * @post On success: isTrained() == true, result has n_components rows and
     *       features.n_cols columns, numFeatures() == features.n_rows
     * @post On failure: isTrained() unchanged (may be set to false),
     *       result is unmodified
     */
    bool fitTransform(
            arma::mat const & features,
            arma::Row<std::size_t> const & labels,
            MLModelParametersBase const * params,
            arma::mat & result) override;

    /**
     * @brief Project new data using the fitted eigenvectors
     *
     * Centers the input using the labeled-subset mean computed during fitting,
     * then projects through the stored eigenvectors.
     *
     * @param features  (D × N) feature matrix — D must match training data
     * @param result    (n_components × N) output
     * @return true on success, false if not trained or dimension mismatch
     *
     * @pre isTrained() must be true (enforcement: runtime_check)
     * @pre features must not be empty (enforcement: runtime_check)
     * @pre features.n_rows must equal numFeatures() (the feature count used
     *      during fitting) (enforcement: runtime_check)
     * @pre features must not contain NaN or Inf — they propagate directly
     *      into the projected result (enforcement: none) [IMPORTANT]
     * @pre If the training data was z-score normalized, features should be
     *      normalized with the SAME mean/std via FeatureConverter::applyZscoreNormalization()
     *      (enforcement: none) [IMPORTANT]
     *
     * @post result has outputDimensions() rows and features.n_cols columns
     */
    bool transform(
            arma::mat const & features,
            arma::mat & result) override;

    // ========================================================================
    // Query / introspection
    // ========================================================================

    [[nodiscard]] std::size_t outputDimensions() const override;
    [[nodiscard]] std::vector<std::string> outputColumnNames() const override;
    [[nodiscard]] bool isTrained() const override;
    [[nodiscard]] std::size_t numClasses() const override;
    [[nodiscard]] std::size_t numFeatures() const override;

    /**
     * @brief Supervised PCA fits only on the positive class
     *
     * Returns false so that the pipeline subsets to label > 0 (the "Inside"
     * class for interval-based labeling) instead of using all observations.
     */
    [[nodiscard]] bool requiresAllClassesForFitting() const override { return false; }

    /**
     * @brief Explained variance ratio per component (of the labeled subset)
     *
     * Each element is the fraction of total variance (in the labeled subset)
     * explained by the corresponding principal component.
     *
     * @return Vector of length outputDimensions(), or empty if not trained
     */
    [[nodiscard]] std::vector<double> explainedVarianceRatio() const;

    // ========================================================================
    // Serialization
    // ========================================================================

    bool save(std::ostream & out) const override;
    bool load(std::istream & in) override;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

}// namespace MLCore

#endif// MLCORE_SUPERVISEDPCAOPERATION_HPP
