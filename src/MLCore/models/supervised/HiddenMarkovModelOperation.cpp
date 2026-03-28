/**
 * @file HiddenMarkovModelOperation.cpp
 * @brief Implementation of Gaussian HMM sequence classification via mlpack
 */

#include "HiddenMarkovModelOperation.hpp"

#include "mlpack.hpp"

#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <iostream>
#include <limits>
#include <optional>
#include <variant>

namespace MLCore {

// ============================================================================
// Pimpl — hides mlpack HMM types from header
// ============================================================================

using GaussianHMM = mlpack::HMM<mlpack::GaussianDistribution<>>;
using DiagonalGaussianHMM = mlpack::HMM<mlpack::DiagonalGaussianDistribution<>>;
using GMMEmissionHMM = mlpack::HMM<mlpack::GMM>;
using DiagonalGMMEmissionHMM = mlpack::HMM<mlpack::DiagonalGMM>;

struct HiddenMarkovModelOperation::Impl {
    std::variant<std::unique_ptr<GaussianHMM>,
                 std::unique_ptr<DiagonalGaussianHMM>,
                 std::unique_ptr<GMMEmissionHMM>,
                 std::unique_ptr<DiagonalGMMEmissionHMM>>
            model;
    std::size_t num_states = 0;
    std::size_t num_features = 0;
    std::size_t num_gaussians = 0;
    bool trained = false;
    bool diagonal = false;
    bool gmm_emissions = false;

    /// @brief Visit the active HMM variant and apply a callable
    template<typename Fn>
    auto visit(Fn && fn) {
        return std::visit(
                [&](auto & ptr) -> decltype(fn(*ptr)) {
                    return fn(*ptr);
                },
                model);
    }

    template<typename Fn>
    auto visit(Fn && fn) const {
        return std::visit(
                [&](auto const & ptr) -> decltype(fn(*ptr)) {
                    return fn(*ptr);
                },
                model);
    }
};

// ============================================================================
// Construction / destruction / move
// ============================================================================

HiddenMarkovModelOperation::HiddenMarkovModelOperation()
    : _impl(std::make_unique<Impl>()) {
}

HiddenMarkovModelOperation::~HiddenMarkovModelOperation() = default;

HiddenMarkovModelOperation::HiddenMarkovModelOperation(HiddenMarkovModelOperation &&) noexcept = default;
HiddenMarkovModelOperation & HiddenMarkovModelOperation::operator=(HiddenMarkovModelOperation &&) noexcept = default;

// ============================================================================
// Identity & metadata
// ============================================================================

std::string HiddenMarkovModelOperation::getName() const {
    return "Hidden Markov Model (Gaussian)";
}

MLTaskType HiddenMarkovModelOperation::getTaskType() const {
    return MLTaskType::BinaryClassification;
}

std::unique_ptr<MLModelParametersBase> HiddenMarkovModelOperation::getDefaultParameters() const {
    return std::make_unique<HMMParameters>();
}

// ============================================================================
// Sequence-based training (native HMM)
// ============================================================================

bool HiddenMarkovModelOperation::trainSequences(
        std::vector<arma::mat> const & featureSequences,
        std::vector<arma::Row<std::size_t>> const & labelSequences,
        MLModelParametersBase const * params) {
    if (featureSequences.empty()) {
        std::cerr << "HiddenMarkovModelOperation::trainSequences: No sequences provided.\n";
        return false;
    }
    if (featureSequences.size() != labelSequences.size()) {
        std::cerr << "HiddenMarkovModelOperation::trainSequences: "
                  << "Feature/label sequence count mismatch.\n";
        return false;
    }

    // Validate each sequence pair
    for (std::size_t i = 0; i < featureSequences.size(); ++i) {
        if (featureSequences[i].n_cols != labelSequences[i].n_elem) {
            std::cerr << "HiddenMarkovModelOperation::trainSequences: "
                      << "Sequence " << i << " column/label count mismatch.\n";
            return false;
        }
        if (featureSequences[i].empty()) {
            std::cerr << "HiddenMarkovModelOperation::trainSequences: "
                      << "Sequence " << i << " is empty.\n";
            return false;
        }
    }

    // Extract parameters
    auto const * hmm_params = dynamic_cast<HMMParameters const *>(params);
    HMMParameters const defaults;
    if (!hmm_params) {
        hmm_params = &defaults;
    }

    std::size_t const num_states = std::max(hmm_params->num_states, std::size_t{2});
    std::size_t const dim = featureSequences[0].n_rows;

    // Validate consistent dimensionality
    for (std::size_t i = 1; i < featureSequences.size(); ++i) {
        if (featureSequences[i].n_rows != dim) {
            std::cerr << "HiddenMarkovModelOperation::trainSequences: "
                      << "Inconsistent feature dimensions across sequences.\n";
            return false;
        }
    }

    try {
        bool const use_diagonal = hmm_params->use_diagonal_covariance;
        bool const use_gmm = hmm_params->use_gmm_emissions;
        std::size_t const num_gaussians_per_state =
                std::max(hmm_params->num_gaussians, std::size_t{2});

        if (use_gmm && use_diagonal) {
            // HMM with DiagonalGMM emissions
            auto hmm = std::make_unique<DiagonalGMMEmissionHMM>(
                    num_states,
                    mlpack::DiagonalGMM(num_gaussians_per_state, dim));
            hmm->Tolerance() = hmm_params->tolerance;
            hmm->Train(featureSequences, labelSequences);
            _impl->model = std::move(hmm);
        } else if (use_gmm) {
            // HMM with full-covariance GMM emissions
            auto hmm = std::make_unique<GMMEmissionHMM>(
                    num_states,
                    mlpack::GMM(num_gaussians_per_state, dim));
            hmm->Tolerance() = hmm_params->tolerance;
            hmm->Train(featureSequences, labelSequences);
            _impl->model = std::move(hmm);
        } else if (use_diagonal) {
            auto hmm = std::make_unique<DiagonalGaussianHMM>(
                    num_states,
                    mlpack::DiagonalGaussianDistribution<>(dim));
            hmm->Tolerance() = hmm_params->tolerance;
            hmm->Train(featureSequences, labelSequences);

            // mlpack's DiagonalGaussianDistribution::Train() does not apply
            // any variance floor (unlike GaussianDistribution which applies
            // PositiveDefiniteConstraint to clamp the condition number).
            // Near-zero variances produce infinite inverse covariance values
            // that dominate the log-likelihood and corrupt Viterbi decoding.
            //
            // Note: mlpack's PositiveDefiniteConstraint vector overload has a
            // bug (initializes maxEigval to -lowest() = +DBL_MAX, so the
            // max-finding loop is a no-op). We implement correct regularization
            // here: clamp each variance so the per-state condition number is
            // at most 1e5, matching the full-covariance constraint behavior.
            for (std::size_t s = 0; s < num_states; ++s) {
                arma::vec cov = hmm->Emission()[s].Covariance();
                double const max_var = cov.max();
                double const floor = std::max(max_var / 1e5, 1e-50);
                cov.transform([floor](double v) {
                    return std::max(v, floor);
                });
                hmm->Emission()[s].Covariance(std::move(cov));
            }

            _impl->model = std::move(hmm);
        } else {
            auto hmm = std::make_unique<GaussianHMM>(
                    num_states,
                    mlpack::GaussianDistribution<>(dim));
            hmm->Tolerance() = hmm_params->tolerance;
            hmm->Train(featureSequences, labelSequences);
            _impl->model = std::move(hmm);
        }

        _impl->num_states = num_states;
        _impl->num_features = dim;
        _impl->num_gaussians = use_gmm ? num_gaussians_per_state : 0;
        _impl->trained = true;
        _impl->diagonal = use_diagonal;
        _impl->gmm_emissions = use_gmm;
        return true;

    } catch (std::exception const & e) {
        std::cerr << "HiddenMarkovModelOperation::trainSequences failed: "
                  << e.what() << "\n";
        _impl->trained = false;
        return false;
    }
}

// ============================================================================
// Sequence-based prediction (Viterbi per sequence)
// ============================================================================

bool HiddenMarkovModelOperation::predictSequences(
        std::vector<arma::mat> const & featureSequences,
        std::vector<arma::Row<std::size_t>> & predictionSequences) {
    if (!_impl->trained) {
        std::cerr << "HiddenMarkovModelOperation::predictSequences: Model not trained.\n";
        return false;
    }
    if (featureSequences.empty()) {
        std::cerr << "HiddenMarkovModelOperation::predictSequences: No sequences provided.\n";
        return false;
    }

    try {
        predictionSequences.clear();
        predictionSequences.reserve(featureSequences.size());

        for (auto const & seq: featureSequences) {
            if (seq.n_rows != _impl->num_features) {
                std::cerr << "HiddenMarkovModelOperation::predictSequences: "
                          << "Feature dimension mismatch — expected "
                          << _impl->num_features << ", got " << seq.n_rows << ".\n";
                return false;
            }

            arma::Row<std::size_t> stateSeq;
            _impl->visit([&](auto & hmm) { hmm.Predict(seq, stateSeq); });
            predictionSequences.push_back(std::move(stateSeq));
        }
        return true;

    } catch (std::exception const & e) {
        std::cerr << "HiddenMarkovModelOperation::predictSequences failed: "
                  << e.what() << "\n";
        return false;
    }
}

bool HiddenMarkovModelOperation::predictSequencesConstrained(
        std::vector<arma::mat> const & featureSequences,
        std::vector<arma::Row<std::size_t>> & predictionSequences,
        std::vector<std::optional<std::size_t>> const & initial_state_constraints) {
    if (!_impl->trained) {
        std::cerr << "HiddenMarkovModelOperation::predictSequencesConstrained: "
                  << "Model not trained.\n";
        return false;
    }
    if (featureSequences.empty()) {
        std::cerr << "HiddenMarkovModelOperation::predictSequencesConstrained: "
                  << "No sequences provided.\n";
        return false;
    }
    if (initial_state_constraints.size() != featureSequences.size()) {
        std::cerr << "HiddenMarkovModelOperation::predictSequencesConstrained: "
                  << "Constraint vector size (" << initial_state_constraints.size()
                  << ") does not match sequence count (" << featureSequences.size()
                  << ").\n";
        return false;
    }

    try {
        predictionSequences.clear();
        predictionSequences.reserve(featureSequences.size());

        // Save the original initial distribution so we can restore it after
        arma::vec const original_initial = _impl->visit(
                [](auto & hmm) -> arma::vec { return hmm.Initial(); });

        for (std::size_t i = 0; i < featureSequences.size(); ++i) {
            auto const & seq = featureSequences[i];

            if (seq.n_rows != _impl->num_features) {
                std::cerr << "HiddenMarkovModelOperation::predictSequencesConstrained: "
                          << "Feature dimension mismatch — expected "
                          << _impl->num_features << ", got " << seq.n_rows << ".\n";
                _impl->visit([&](auto & hmm) { hmm.Initial() = original_initial; });
                return false;
            }

            if (initial_state_constraints[i].has_value()) {
                std::size_t const state = *initial_state_constraints[i];
                if (state >= _impl->num_states) {
                    std::cerr << "HiddenMarkovModelOperation::predictSequencesConstrained: "
                              << "Constraint state " << state
                              << " >= num_states (" << _impl->num_states << ").\n";
                    _impl->visit([&](auto & hmm) { hmm.Initial() = original_initial; });
                    return false;
                }
                // Clamp initial distribution to a delta on the constrained state
                arma::vec constrained(_impl->num_states, arma::fill::zeros);
                constrained[state] = 1.0;
                _impl->visit([&](auto & hmm) { hmm.Initial() = constrained; });
            } else {
                // Restore default distribution for unconstrained segments
                _impl->visit([&](auto & hmm) { hmm.Initial() = original_initial; });
            }

            arma::Row<std::size_t> stateSeq;
            _impl->visit([&](auto & hmm) { hmm.Predict(seq, stateSeq); });
            predictionSequences.push_back(std::move(stateSeq));
        }

        // Restore the original initial distribution
        _impl->visit([&](auto & hmm) { hmm.Initial() = original_initial; });
        return true;

    } catch (std::exception const & e) {
        std::cerr << "HiddenMarkovModelOperation::predictSequencesConstrained failed: "
                  << e.what() << "\n";
        return false;
    }
}

bool HiddenMarkovModelOperation::isSequenceModel() const {
    return true;
}

// ============================================================================
// Frame-based fallbacks (treat entire input as one sequence)
// ============================================================================

bool HiddenMarkovModelOperation::train(
        arma::mat const & features,
        arma::Row<std::size_t> const & labels,
        MLModelParametersBase const * params) {
    // Wrap in single-sequence vectors and delegate
    std::vector<arma::mat> const seqs{features};
    std::vector<arma::Row<std::size_t>> const labelSeqs{labels};
    return trainSequences(seqs, labelSeqs, params);
}

bool HiddenMarkovModelOperation::predict(
        arma::mat const & features,
        arma::Row<std::size_t> & predictions) {
    if (!_impl->trained) {
        std::cerr << "HiddenMarkovModelOperation::predict: Model not trained.\n";
        return false;
    }
    if (features.empty()) {
        std::cerr << "HiddenMarkovModelOperation::predict: Feature matrix is empty.\n";
        return false;
    }
    if (features.n_rows != _impl->num_features) {
        std::cerr << "HiddenMarkovModelOperation::predict: Feature dimension mismatch — "
                  << "expected " << _impl->num_features
                  << ", got " << features.n_rows << ".\n";
        return false;
    }

    try {
        _impl->visit([&](auto & hmm) { hmm.Predict(features, predictions); });
        return true;
    } catch (std::exception const & e) {
        std::cerr << "HiddenMarkovModelOperation::predict failed: " << e.what() << "\n";
        return false;
    }
}

bool HiddenMarkovModelOperation::predictProbabilities(
        arma::mat const & features,
        arma::mat & probabilities) {
    if (!_impl->trained) {
        std::cerr << "HiddenMarkovModelOperation::predictProbabilities: Model not trained.\n";
        return false;
    }
    if (features.empty()) {
        std::cerr << "HiddenMarkovModelOperation::predictProbabilities: Feature matrix is empty.\n";
        return false;
    }
    if (features.n_rows != _impl->num_features) {
        std::cerr << "HiddenMarkovModelOperation::predictProbabilities: "
                  << "Feature dimension mismatch — expected "
                  << _impl->num_features << ", got " << features.n_rows << ".\n";
        return false;
    }

    try {
        // Forward-Backward: Estimate() produces (num_states × observations)
        _impl->visit([&](auto & hmm) { hmm.Estimate(features, probabilities); });
        return true;
    } catch (std::exception const & e) {
        std::cerr << "HiddenMarkovModelOperation::predictProbabilities failed: "
                  << e.what() << "\n";
        return false;
    }
}

// ============================================================================
// Serialization
// ============================================================================

bool HiddenMarkovModelOperation::save(std::ostream & out) const {
    if (!_impl->trained) {
        return false;
    }

    try {
        cereal::BinaryOutputArchive ar(out);
        ar(cereal::make_nvp("num_states", _impl->num_states));
        ar(cereal::make_nvp("num_features", _impl->num_features));
        ar(cereal::make_nvp("diagonal", _impl->diagonal));
        ar(cereal::make_nvp("gmm_emissions", _impl->gmm_emissions));
        ar(cereal::make_nvp("num_gaussians", _impl->num_gaussians));
        _impl->visit([&](auto const & hmm) {
            ar(cereal::make_nvp("model", hmm));
        });
        return out.good();
    } catch (std::exception const & e) {
        std::cerr << "HiddenMarkovModelOperation::save failed: " << e.what() << "\n";
        return false;
    }
}

bool HiddenMarkovModelOperation::load(std::istream & in) {
    try {
        cereal::BinaryInputArchive ar(in);

        std::size_t num_states = 0;
        std::size_t num_features = 0;
        bool diagonal = false;
        bool gmm_emissions = false;
        std::size_t num_gaussians = 0;
        ar(cereal::make_nvp("num_states", num_states));
        ar(cereal::make_nvp("num_features", num_features));
        ar(cereal::make_nvp("diagonal", diagonal));
        ar(cereal::make_nvp("gmm_emissions", gmm_emissions));
        ar(cereal::make_nvp("num_gaussians", num_gaussians));

        if (gmm_emissions && diagonal) {
            auto model = std::make_unique<DiagonalGMMEmissionHMM>();
            ar(cereal::make_nvp("model", *model));
            _impl->model = std::move(model);
        } else if (gmm_emissions) {
            auto model = std::make_unique<GMMEmissionHMM>();
            ar(cereal::make_nvp("model", *model));
            _impl->model = std::move(model);
        } else if (diagonal) {
            auto model = std::make_unique<DiagonalGaussianHMM>();
            ar(cereal::make_nvp("model", *model));
            _impl->model = std::move(model);
        } else {
            auto model = std::make_unique<GaussianHMM>();
            ar(cereal::make_nvp("model", *model));
            _impl->model = std::move(model);
        }

        _impl->num_states = num_states;
        _impl->num_features = num_features;
        _impl->num_gaussians = num_gaussians;
        _impl->diagonal = diagonal;
        _impl->gmm_emissions = gmm_emissions;
        _impl->trained = true;
        return true;
    } catch (std::exception const & e) {
        std::cerr << "HiddenMarkovModelOperation::load failed: " << e.what() << "\n";
        _impl->trained = false;
        return false;
    }
}

// ============================================================================
// Query / introspection
// ============================================================================

bool HiddenMarkovModelOperation::isTrained() const {
    return _impl->trained;
}

std::size_t HiddenMarkovModelOperation::numClasses() const {
    return _impl->trained ? _impl->num_states : 0;
}

std::size_t HiddenMarkovModelOperation::numFeatures() const {
    return _impl->trained ? _impl->num_features : 0;
}

arma::mat HiddenMarkovModelOperation::getTransitionMatrix() const {
    if (!_impl->trained) {
        return {};
    }
    return _impl->visit([](auto const & hmm) -> arma::mat {
        return hmm.Transition();
    });
}

double HiddenMarkovModelOperation::logLikelihood(arma::mat const & features) const {
    if (!_impl->trained) {
        return -std::numeric_limits<double>::infinity();
    }
    if (features.empty() || features.n_rows != _impl->num_features) {
        return -std::numeric_limits<double>::infinity();
    }
    try {
        return _impl->visit([&](auto const & hmm) -> double {
            return hmm.LogLikelihood(features);
        });
    } catch (std::exception const & e) {
        std::cerr << "HiddenMarkovModelOperation::logLikelihood failed: "
                  << e.what() << "\n";
        return -std::numeric_limits<double>::infinity();
    }
}

bool HiddenMarkovModelOperation::isDiagonalCovariance() const {
    return _impl->diagonal;
}

bool HiddenMarkovModelOperation::isGMMEmissions() const {
    return _impl->gmm_emissions;
}

std::size_t HiddenMarkovModelOperation::numGaussiansPerState() const {
    return _impl->gmm_emissions ? _impl->num_gaussians : 0;
}

}// namespace MLCore
