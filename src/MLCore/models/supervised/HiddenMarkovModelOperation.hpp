#ifndef MLCORE_HIDDENMARKOVMODELOPERATION_HPP
#define MLCORE_HIDDENMARKOVMODELOPERATION_HPP

/**
 * @file HiddenMarkovModelOperation.hpp
 * @brief 2-state Gaussian HMM for temporal sequence classification
 *
 * Wraps mlpack::HMM<mlpack::GaussianDistribution<>> to provide a
 * sequence-aware classification model. Uses supervised training with
 * labeled temporal sequences and Viterbi decoding for prediction.
 *
 * Key differences from frame-based classifiers:
 * - Exploits temporal continuity via transition probabilities
 * - Operates on pre-segmented contiguous sequences (SequenceAssembler)
 * - Overrides trainSequences() / predictSequences() for native HMM behavior
 * - Supports Forward-Backward probability estimation via predictProbabilities()
 */

#include "MLCore/models/MLModelOperation.hpp"
#include "MLCore/models/MLModelParameters.hpp"
#include "MLCore/models/MLTaskType.hpp"

#include <armadillo>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace MLCore {

/**
 * @brief Gaussian HMM classifier wrapping mlpack::HMM<GaussianDistribution<>>
 *
 * Supervised sequence classification using a Hidden Markov Model with
 * multivariate Gaussian emission distributions. Each hidden state has
 * its own mean vector and covariance matrix learned from training data.
 *
 * ## Usage
 * ```cpp
 * HiddenMarkovModelOperation hmm;
 * HMMParameters params;
 * params.num_states = 2;
 *
 * // Sequence-based training (preferred)
 * hmm.trainSequences(featureSeqs, labelSeqs, &params);
 * hmm.predictSequences(testSeqs, predSeqs);
 *
 * // Probability output (Forward-Backward)
 * arma::mat probs;
 * hmm.predictProbabilities(features, probs);
 * ```
 */
class HiddenMarkovModelOperation : public MLModelOperation {
public:
    HiddenMarkovModelOperation();
    ~HiddenMarkovModelOperation() override;

    // Non-copyable (owns mlpack model)
    HiddenMarkovModelOperation(HiddenMarkovModelOperation const &) = delete;
    HiddenMarkovModelOperation & operator=(HiddenMarkovModelOperation const &) = delete;

    // Movable
    HiddenMarkovModelOperation(HiddenMarkovModelOperation &&) noexcept;
    HiddenMarkovModelOperation & operator=(HiddenMarkovModelOperation &&) noexcept;

    // ========================================================================
    // Identity & metadata
    // ========================================================================

    [[nodiscard]] std::string getName() const override;
    [[nodiscard]] MLTaskType getTaskType() const override;
    [[nodiscard]] std::unique_ptr<MLModelParametersBase> getDefaultParameters() const override;

    // ========================================================================
    // Supervised training & prediction (frame-based fallbacks)
    // ========================================================================

    /// @brief Train on concatenated data (treats as single sequence)
    bool train(
            arma::mat const & features,
            arma::Row<std::size_t> const & labels,
            MLModelParametersBase const * params) override;

    /// @brief Predict via Viterbi decoding (treats input as single sequence)
    bool predict(
            arma::mat const & features,
            arma::Row<std::size_t> & predictions) override;

    /// @brief Forward-Backward state probabilities (num_states × observations)
    bool predictProbabilities(
            arma::mat const & features,
            arma::mat & probabilities) override;

    // ========================================================================
    // Sequence-based training & prediction (native HMM behavior)
    // ========================================================================

    /// @brief Train on pre-segmented temporal sequences
    bool trainSequences(
            std::vector<arma::mat> const & featureSequences,
            std::vector<arma::Row<std::size_t>> const & labelSequences,
            MLModelParametersBase const * params) override;

    /// @brief Viterbi decode each sequence independently
    bool predictSequences(
            std::vector<arma::mat> const & featureSequences,
            std::vector<arma::Row<std::size_t>> & predictionSequences) override;

    /// @brief Viterbi decode with per-sequence initial state constraints
    bool predictSequencesConstrained(
            std::vector<arma::mat> const & featureSequences,
            std::vector<arma::Row<std::size_t>> & predictionSequences,
            std::vector<std::optional<std::size_t>> const & initial_state_constraints) override;

    /// @brief Custom Viterbi with both initial and terminal state constraints
    bool predictSequencesBidirectionalConstrained(
            std::vector<arma::mat> const & featureSequences,
            std::vector<arma::Row<std::size_t>> & predictionSequences,
            std::vector<std::optional<std::size_t>> const & initial_state_constraints,
            std::vector<std::optional<std::size_t>> const & terminal_state_constraints) override;

    /// @brief Per-sequence Forward-Backward with boundary state constraints
    bool predictProbabilitiesPerSequence(
            std::vector<arma::mat> const & featureSequences,
            std::vector<arma::mat> & probabilitySequences,
            std::vector<std::optional<std::size_t>> const & initial_state_constraints,
            std::vector<std::optional<std::size_t>> const & terminal_state_constraints) override;

    /// @brief This model exploits temporal structure
    [[nodiscard]] bool isSequenceModel() const override;

    // ========================================================================
    // Serialization
    // ========================================================================

    bool save(std::ostream & out) const override;
    bool load(std::istream & in) override;

    // ========================================================================
    // Query / introspection
    // ========================================================================

    [[nodiscard]] bool isTrained() const override;
    [[nodiscard]] std::size_t numClasses() const override;
    [[nodiscard]] std::size_t numFeatures() const override;

    /**
     * @brief Get the learned transition matrix
     * @return Transition matrix (num_states × num_states) where entry (i,j) is P(state j | state i).
     *         Returns empty matrix if not trained.
     */
    [[nodiscard]] arma::mat getTransitionMatrix() const;

    /**
     * @brief Compute log-likelihood of an observation sequence
     * @param features Feature matrix (features × observations)
     * @return Log-likelihood value, or -infinity if not trained
     */
    [[nodiscard]] double logLikelihood(arma::mat const & features) const;

    /**
     * @brief Whether the model uses diagonal covariance emissions
     * @return true if trained with use_diagonal_covariance, false otherwise
     */
    [[nodiscard]] bool isDiagonalCovariance() const;

    /**
     * @brief Whether the model uses GMM (mixture of Gaussians) emissions
     * @return true if trained with use_gmm_emissions, false otherwise
     */
    [[nodiscard]] bool isGMMEmissions() const;

    /**
     * @brief Number of Gaussian components per state (GMM emissions only)
     * @return Number of Gaussians per state, or 0 if not using GMM emissions
     */
    [[nodiscard]] std::size_t numGaussiansPerState() const;

private:
    /// Pimpl to keep mlpack HMM headers out of this header
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

}// namespace MLCore

#endif// MLCORE_HIDDENMARKOVMODELOPERATION_HPP
