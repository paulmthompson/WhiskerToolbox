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

namespace MLCore {

// ============================================================================
// Pimpl — hides mlpack HMM types from header
// ============================================================================

using GaussianHMM = mlpack::HMM<mlpack::GaussianDistribution<>>;

struct HiddenMarkovModelOperation::Impl {
    std::unique_ptr<GaussianHMM> model;
    std::size_t num_states = 0;
    std::size_t num_features = 0;
    bool trained = false;
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
        // Construct HMM with the correct dimensionality
        auto hmm = std::make_unique<GaussianHMM>(
                num_states,
                mlpack::GaussianDistribution<>(dim));
        hmm->Tolerance() = hmm_params->tolerance;

        // mlpack supervised Train expects vector<mat> and vector<Row<size_t>>
        hmm->Train(featureSequences, labelSequences);

        _impl->model = std::move(hmm);
        _impl->num_states = num_states;
        _impl->num_features = dim;
        _impl->trained = true;
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
    if (!_impl->trained || !_impl->model) {
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
            _impl->model->Predict(seq, stateSeq);
            predictionSequences.push_back(std::move(stateSeq));
        }
        return true;

    } catch (std::exception const & e) {
        std::cerr << "HiddenMarkovModelOperation::predictSequences failed: "
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
    if (!_impl->trained || !_impl->model) {
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
        _impl->model->Predict(features, predictions);
        return true;
    } catch (std::exception const & e) {
        std::cerr << "HiddenMarkovModelOperation::predict failed: " << e.what() << "\n";
        return false;
    }
}

bool HiddenMarkovModelOperation::predictProbabilities(
        arma::mat const & features,
        arma::mat & probabilities) {
    if (!_impl->trained || !_impl->model) {
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
        _impl->model->Estimate(features, probabilities);
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
    if (!_impl->trained || !_impl->model) {
        return false;
    }

    try {
        cereal::BinaryOutputArchive ar(out);
        ar(cereal::make_nvp("num_states", _impl->num_states));
        ar(cereal::make_nvp("num_features", _impl->num_features));
        ar(cereal::make_nvp("model", *_impl->model));
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
        ar(cereal::make_nvp("num_states", num_states));
        ar(cereal::make_nvp("num_features", num_features));

        auto model = std::make_unique<GaussianHMM>();
        ar(cereal::make_nvp("model", *model));

        _impl->model = std::move(model);
        _impl->num_states = num_states;
        _impl->num_features = num_features;
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
    if (!_impl->trained || !_impl->model) {
        return {};
    }
    return _impl->model->Transition();
}

double HiddenMarkovModelOperation::logLikelihood(arma::mat const & features) const {
    if (!_impl->trained || !_impl->model) {
        return -std::numeric_limits<double>::infinity();
    }
    if (features.empty() || features.n_rows != _impl->num_features) {
        return -std::numeric_limits<double>::infinity();
    }
    try {
        return _impl->model->LogLikelihood(features);
    } catch (std::exception const & e) {
        std::cerr << "HiddenMarkovModelOperation::logLikelihood failed: "
                  << e.what() << "\n";
        return -std::numeric_limits<double>::infinity();
    }
}

}// namespace MLCore
