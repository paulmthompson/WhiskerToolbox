/**
 * @file HiddenMarkovModel.test.cpp
 * @brief Catch2 tests for MLCore::HiddenMarkovModelOperation
 *
 * Tests the Gaussian HMM implementation against synthetic temporal data
 * with known state transitions. Covers:
 * - Metadata (name, task type, default parameters)
 * - Sequence-based training and Viterbi decoding
 * - Frame-based fallback (train/predict)
 * - Forward-Backward probability estimation
 * - Transition matrix inspection
 * - Log-likelihood computation
 * - Feature dimension validation
 * - Edge cases (empty data, untrained model)
 * - Save/load round-trip
 * - Multi-dimensional emissions
 * - Registry integration
 */

#include "models/MLModelParameters.hpp"
#include "models/MLModelRegistry.hpp"
#include "models/MLTaskType.hpp"
#include "models/supervised/HiddenMarkovModelOperation.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <armadillo>

#include <sstream>

using namespace MLCore;

// ============================================================================
// Helper: create synthetic 2-state HMM data
// ============================================================================

namespace {

/**
 * @brief Generate a 2-state temporal sequence with distinct Gaussian emissions
 *
 * State 0: emission centered around -2.0 (1D)
 * State 1: emission centered around +2.0 (1D)
 *
 * The state sequence alternates in blocks to create clear temporal structure:
 * [0,0,...,0, 1,1,...,1, 0,0,...,0, 1,1,...,1]
 *
 * @param block_size Number of frames per state block
 * @param num_blocks Number of state-0/state-1 pairs
 * @param seed       Random seed for reproducibility
 */
struct SyntheticSequenceData {
    arma::mat features;
    arma::Row<std::size_t> labels;
};

SyntheticSequenceData makeBlockSequence(
        std::size_t block_size = 50,
        std::size_t num_blocks = 4,
        int seed = 42) {
    arma::arma_rng::set_seed(seed);

    std::size_t const total = block_size * num_blocks * 2;
    SyntheticSequenceData data;
    data.features.set_size(1, total);
    data.labels.set_size(total);

    std::size_t col = 0;
    for (std::size_t b = 0; b < num_blocks; ++b) {
        // State 0 block
        for (std::size_t i = 0; i < block_size; ++i) {
            data.features(0, col) = -2.0 + arma::randn() * 0.3;
            data.labels(col) = 0;
            ++col;
        }
        // State 1 block
        for (std::size_t i = 0; i < block_size; ++i) {
            data.features(0, col) = +2.0 + arma::randn() * 0.3;
            data.labels(col) = 1;
            ++col;
        }
    }

    return data;
}

/**
 * @brief Generate multi-dimensional 2-state sequence data
 *
 * State 0: emissions centered around (-2, -1) in 2D
 * State 1: emissions centered around (+2, +1) in 2D
 */
SyntheticSequenceData makeMultiDimBlockSequence(
        std::size_t block_size = 40,
        std::size_t num_blocks = 3,
        int seed = 42) {
    arma::arma_rng::set_seed(seed);

    std::size_t const total = block_size * num_blocks * 2;
    SyntheticSequenceData data;
    data.features.set_size(2, total);
    data.labels.set_size(total);

    std::size_t col = 0;
    for (std::size_t b = 0; b < num_blocks; ++b) {
        for (std::size_t i = 0; i < block_size; ++i) {
            data.features(0, col) = -2.0 + arma::randn() * 0.3;
            data.features(1, col) = -1.0 + arma::randn() * 0.3;
            data.labels(col) = 0;
            ++col;
        }
        for (std::size_t i = 0; i < block_size; ++i) {
            data.features(0, col) = +2.0 + arma::randn() * 0.3;
            data.features(1, col) = +1.0 + arma::randn() * 0.3;
            data.labels(col) = 1;
            ++col;
        }
    }

    return data;
}

}// anonymous namespace

// ============================================================================
// Metadata tests
// ============================================================================

TEST_CASE("HiddenMarkovModelOperation - metadata", "[MLCore][HMM]") {
    HiddenMarkovModelOperation hmm;

    SECTION("name is 'Hidden Markov Model (Gaussian)'") {
        CHECK(hmm.getName() == "Hidden Markov Model (Gaussian)");
    }

    SECTION("task type is BinaryClassification") {
        CHECK(hmm.getTaskType() == MLTaskType::BinaryClassification);
    }

    SECTION("default parameters are HMMParameters") {
        auto params = hmm.getDefaultParameters();
        REQUIRE(params != nullptr);

        auto const * hmm_params = dynamic_cast<HMMParameters const *>(params.get());
        REQUIRE(hmm_params != nullptr);

        CHECK(hmm_params->num_states == 2);
        CHECK_THAT(hmm_params->tolerance,
                   Catch::Matchers::WithinAbs(1e-5, 1e-10));
    }

    SECTION("not trained initially") {
        CHECK_FALSE(hmm.isTrained());
        CHECK(hmm.numClasses() == 0);
        CHECK(hmm.numFeatures() == 0);
    }

    SECTION("isSequenceModel returns true") {
        CHECK(hmm.isSequenceModel());
    }
}

// ============================================================================
// Sequence-based training tests
// ============================================================================

TEST_CASE("HiddenMarkovModelOperation - trainSequences", "[MLCore][HMM]") {
    HiddenMarkovModelOperation hmm;
    auto data = makeBlockSequence(50, 4, 42);

    SECTION("train with single sequence and default parameters") {
        std::vector<arma::mat> seqs{data.features};
        std::vector<arma::Row<std::size_t>> labelSeqs{data.labels};

        bool ok = hmm.trainSequences(seqs, labelSeqs, nullptr);
        CHECK(ok);
        CHECK(hmm.isTrained());
        CHECK(hmm.numClasses() == 2);
        CHECK(hmm.numFeatures() == 1);
    }

    SECTION("train with multiple sequences") {
        // Split into two separate sequences
        std::size_t const half = data.features.n_cols / 2;
        arma::mat seq1 = data.features.cols(0, half - 1);
        arma::mat seq2 = data.features.cols(half, data.features.n_cols - 1);
        arma::Row<std::size_t> lab1 = data.labels.subvec(0, half - 1);
        arma::Row<std::size_t> lab2 = data.labels.subvec(half, data.labels.n_elem - 1);

        std::vector<arma::mat> seqs{seq1, seq2};
        std::vector<arma::Row<std::size_t>> labelSeqs{lab1, lab2};

        bool ok = hmm.trainSequences(seqs, labelSeqs, nullptr);
        CHECK(ok);
        CHECK(hmm.isTrained());
    }

    SECTION("train with explicit parameters") {
        HMMParameters params;
        params.num_states = 2;
        params.tolerance = 1e-4;

        std::vector<arma::mat> seqs{data.features};
        std::vector<arma::Row<std::size_t>> labelSeqs{data.labels};

        bool ok = hmm.trainSequences(seqs, labelSeqs, &params);
        CHECK(ok);
        CHECK(hmm.isTrained());
    }
}

// ============================================================================
// Sequence training error handling
// ============================================================================

TEST_CASE("HiddenMarkovModelOperation - trainSequences error handling", "[MLCore][HMM]") {
    HiddenMarkovModelOperation hmm;

    SECTION("empty sequence vector") {
        std::vector<arma::mat> empty_seqs;
        std::vector<arma::Row<std::size_t>> empty_labels;
        CHECK_FALSE(hmm.trainSequences(empty_seqs, empty_labels, nullptr));
        CHECK_FALSE(hmm.isTrained());
    }

    SECTION("feature/label sequence count mismatch") {
        arma::mat feat(1, 10, arma::fill::randn);
        std::vector<arma::mat> seqs{feat};
        std::vector<arma::Row<std::size_t>> labels;// empty - count mismatch
        CHECK_FALSE(hmm.trainSequences(seqs, labels, nullptr));
    }

    SECTION("column/label count mismatch within a sequence") {
        arma::mat feat(1, 10, arma::fill::randn);
        arma::Row<std::size_t> lab(5, arma::fill::zeros);// mismatch
        std::vector<arma::mat> seqs{feat};
        std::vector<arma::Row<std::size_t>> labels{lab};
        CHECK_FALSE(hmm.trainSequences(seqs, labels, nullptr));
    }

    SECTION("inconsistent feature dimensions across sequences") {
        arma::mat feat1(1, 10, arma::fill::randn);
        arma::mat feat2(2, 10, arma::fill::randn);// different dim
        arma::Row<std::size_t> lab(10, arma::fill::zeros);
        std::vector<arma::mat> seqs{feat1, feat2};
        std::vector<arma::Row<std::size_t>> labels{lab, lab};
        CHECK_FALSE(hmm.trainSequences(seqs, labels, nullptr));
    }
}

// ============================================================================
// Frame-based fallback training
// ============================================================================

TEST_CASE("HiddenMarkovModelOperation - frame-based train/predict", "[MLCore][HMM]") {
    HiddenMarkovModelOperation hmm;
    auto data = makeBlockSequence(50, 4, 42);

    SECTION("train() wraps input as single sequence") {
        bool ok = hmm.train(data.features, data.labels, nullptr);
        CHECK(ok);
        CHECK(hmm.isTrained());
        CHECK(hmm.numClasses() == 2);
        CHECK(hmm.numFeatures() == 1);
    }

    SECTION("predict() on trained model produces correct labels") {
        hmm.train(data.features, data.labels, nullptr);
        REQUIRE(hmm.isTrained());

        arma::Row<std::size_t> predictions;
        bool ok = hmm.predict(data.features, predictions);
        REQUIRE(ok);
        REQUIRE(predictions.n_elem == data.labels.n_elem);

        // Count correct predictions — HMM should recover most states
        // on well-separated data
        std::size_t correct = arma::accu(predictions == data.labels);
        double accuracy = static_cast<double>(correct) / data.labels.n_elem;
        CHECK(accuracy > 0.90);
    }
}

// ============================================================================
// Sequence-based prediction (Viterbi)
// ============================================================================

TEST_CASE("HiddenMarkovModelOperation - predictSequences Viterbi decoding",
          "[MLCore][HMM]") {
    HiddenMarkovModelOperation hmm;
    auto data = makeBlockSequence(50, 4, 42);

    // Train on the full sequence
    std::vector<arma::mat> trainSeqs{data.features};
    std::vector<arma::Row<std::size_t>> trainLabels{data.labels};
    REQUIRE(hmm.trainSequences(trainSeqs, trainLabels, nullptr));

    SECTION("predict recovers block states with high accuracy") {
        std::vector<arma::Row<std::size_t>> predSeqs;
        bool ok = hmm.predictSequences(trainSeqs, predSeqs);
        REQUIRE(ok);
        REQUIRE(predSeqs.size() == 1);
        REQUIRE(predSeqs[0].n_elem == data.labels.n_elem);

        std::size_t correct = arma::accu(predSeqs[0] == data.labels);
        double accuracy = static_cast<double>(correct) / data.labels.n_elem;
        CHECK(accuracy > 0.90);
    }

    SECTION("predict on multiple sequences") {
        std::size_t const half = data.features.n_cols / 2;
        arma::mat seq1 = data.features.cols(0, half - 1);
        arma::mat seq2 = data.features.cols(half, data.features.n_cols - 1);

        std::vector<arma::mat> testSeqs{seq1, seq2};
        std::vector<arma::Row<std::size_t>> predSeqs;
        bool ok = hmm.predictSequences(testSeqs, predSeqs);
        REQUIRE(ok);
        REQUIRE(predSeqs.size() == 2);
        CHECK(predSeqs[0].n_elem == half);
        CHECK(predSeqs[1].n_elem == data.features.n_cols - half);
    }
}

// ============================================================================
// Prediction error handling
// ============================================================================

TEST_CASE("HiddenMarkovModelOperation - predict error handling", "[MLCore][HMM]") {
    HiddenMarkovModelOperation hmm;

    SECTION("predict on untrained model fails") {
        arma::mat features(1, 10, arma::fill::randn);
        arma::Row<std::size_t> predictions;
        CHECK_FALSE(hmm.predict(features, predictions));
    }

    SECTION("predictSequences on untrained model fails") {
        arma::mat features(1, 10, arma::fill::randn);
        std::vector<arma::mat> seqs{features};
        std::vector<arma::Row<std::size_t>> predSeqs;
        CHECK_FALSE(hmm.predictSequences(seqs, predSeqs));
    }

    SECTION("predict with empty features fails") {
        auto data = makeBlockSequence(20, 2, 42);
        hmm.train(data.features, data.labels, nullptr);
        REQUIRE(hmm.isTrained());

        arma::mat empty_features;
        arma::Row<std::size_t> predictions;
        CHECK_FALSE(hmm.predict(empty_features, predictions));
    }

    SECTION("predict with dimension mismatch fails") {
        auto data = makeBlockSequence(20, 2, 42);
        hmm.train(data.features, data.labels, nullptr);
        REQUIRE(hmm.isTrained());

        arma::mat wrong_dim(3, 10, arma::fill::randn);
        arma::Row<std::size_t> predictions;
        CHECK_FALSE(hmm.predict(wrong_dim, predictions));
    }

    SECTION("predictSequences with dimension mismatch fails") {
        auto data = makeBlockSequence(20, 2, 42);
        hmm.train(data.features, data.labels, nullptr);
        REQUIRE(hmm.isTrained());

        arma::mat wrong_dim(3, 10, arma::fill::randn);
        std::vector<arma::mat> seqs{wrong_dim};
        std::vector<arma::Row<std::size_t>> predSeqs;
        CHECK_FALSE(hmm.predictSequences(seqs, predSeqs));
    }
}

// ============================================================================
// Forward-Backward probability estimation
// ============================================================================

TEST_CASE("HiddenMarkovModelOperation - predictProbabilities (Forward-Backward)",
          "[MLCore][HMM]") {
    HiddenMarkovModelOperation hmm;
    auto data = makeBlockSequence(50, 4, 42);
    hmm.train(data.features, data.labels, nullptr);
    REQUIRE(hmm.isTrained());

    SECTION("returns matrix of correct dimensions") {
        arma::mat probs;
        bool ok = hmm.predictProbabilities(data.features, probs);
        REQUIRE(ok);
        CHECK(probs.n_rows == 2);// num_states
        CHECK(probs.n_cols == data.features.n_cols);
    }

    SECTION("probabilities sum to approximately 1 per column") {
        arma::mat probs;
        hmm.predictProbabilities(data.features, probs);

        arma::rowvec col_sums = arma::sum(probs, 0);
        for (std::size_t c = 0; c < col_sums.n_elem; ++c) {
            CHECK_THAT(col_sums(c), Catch::Matchers::WithinAbs(1.0, 0.01));
        }
    }

    SECTION("high state-0 probability in state-0 regions") {
        arma::mat probs;
        hmm.predictProbabilities(data.features, probs);

        // First block (indices 0–49) should be state 0
        double avg_state0_prob = arma::mean(probs.row(0).subvec(10, 40));
        CHECK(avg_state0_prob > 0.8);
    }

    SECTION("fails on untrained model") {
        HiddenMarkovModelOperation untrained;
        arma::mat probs;
        CHECK_FALSE(untrained.predictProbabilities(data.features, probs));
    }

    SECTION("fails on empty features") {
        arma::mat empty_features;
        arma::mat probs;
        CHECK_FALSE(hmm.predictProbabilities(empty_features, probs));
    }

    SECTION("fails on dimension mismatch") {
        arma::mat wrong_dim(3, 10, arma::fill::randn);
        arma::mat probs;
        CHECK_FALSE(hmm.predictProbabilities(wrong_dim, probs));
    }
}

// ============================================================================
// Transition matrix inspection
// ============================================================================

TEST_CASE("HiddenMarkovModelOperation - transition matrix", "[MLCore][HMM]") {
    HiddenMarkovModelOperation hmm;
    auto data = makeBlockSequence(50, 4, 42);
    hmm.train(data.features, data.labels, nullptr);
    REQUIRE(hmm.isTrained());

    SECTION("transition matrix has correct dimensions") {
        arma::mat trans = hmm.getTransitionMatrix();
        CHECK(trans.n_rows == 2);
        CHECK(trans.n_cols == 2);
    }

    SECTION("transition matrix columns sum to 1") {
        arma::mat trans = hmm.getTransitionMatrix();
        arma::rowvec col_sums = arma::sum(trans, 0);
        CHECK_THAT(col_sums(0), Catch::Matchers::WithinAbs(1.0, 0.01));
        CHECK_THAT(col_sums(1), Catch::Matchers::WithinAbs(1.0, 0.01));
    }

    SECTION("diagonal dominance (states tend to persist)") {
        arma::mat trans = hmm.getTransitionMatrix();
        // With block_size=50, self-transition should be high
        CHECK(trans(0, 0) > 0.8);
        CHECK(trans(1, 1) > 0.8);
    }

    SECTION("untrained model returns empty matrix") {
        HiddenMarkovModelOperation untrained;
        arma::mat trans = untrained.getTransitionMatrix();
        CHECK(trans.empty());
    }
}

// ============================================================================
// Log-likelihood
// ============================================================================

TEST_CASE("HiddenMarkovModelOperation - logLikelihood", "[MLCore][HMM]") {
    HiddenMarkovModelOperation hmm;
    auto data = makeBlockSequence(50, 4, 42);
    hmm.train(data.features, data.labels, nullptr);
    REQUIRE(hmm.isTrained());

    SECTION("returns finite value on training data") {
        double ll = hmm.logLikelihood(data.features);
        CHECK(std::isfinite(ll));
    }

    SECTION("untrained model returns -infinity") {
        HiddenMarkovModelOperation untrained;
        double ll = untrained.logLikelihood(data.features);
        CHECK(ll == -std::numeric_limits<double>::infinity());
    }

    SECTION("empty features returns -infinity") {
        arma::mat empty_features;
        double ll = hmm.logLikelihood(empty_features);
        CHECK(ll == -std::numeric_limits<double>::infinity());
    }

    SECTION("dimension mismatch returns -infinity") {
        arma::mat wrong_dim(3, 10, arma::fill::randn);
        double ll = hmm.logLikelihood(wrong_dim);
        CHECK(ll == -std::numeric_limits<double>::infinity());
    }
}

// ============================================================================
// Multi-dimensional emissions
// ============================================================================

TEST_CASE("HiddenMarkovModelOperation - multi-dimensional emissions",
          "[MLCore][HMM]") {
    HiddenMarkovModelOperation hmm;
    auto data = makeMultiDimBlockSequence(40, 3, 42);

    SECTION("trains on 2D emission data") {
        bool ok = hmm.train(data.features, data.labels, nullptr);
        CHECK(ok);
        CHECK(hmm.numFeatures() == 2);
        CHECK(hmm.numClasses() == 2);
    }

    SECTION("predicts with high accuracy on 2D data") {
        hmm.train(data.features, data.labels, nullptr);
        REQUIRE(hmm.isTrained());

        arma::Row<std::size_t> predictions;
        hmm.predict(data.features, predictions);

        std::size_t correct = arma::accu(predictions == data.labels);
        double accuracy = static_cast<double>(correct) / data.labels.n_elem;
        CHECK(accuracy > 0.90);
    }

    SECTION("probability output has correct shape") {
        hmm.train(data.features, data.labels, nullptr);
        arma::mat probs;
        hmm.predictProbabilities(data.features, probs);
        CHECK(probs.n_rows == 2);
        CHECK(probs.n_cols == data.features.n_cols);
    }
}

// ============================================================================
// Save/load round-trip
// ============================================================================

TEST_CASE("HiddenMarkovModelOperation - save/load round-trip", "[MLCore][HMM]") {
    auto data = makeBlockSequence(50, 4, 42);

    SECTION("round-trip preserves predictions") {
        HiddenMarkovModelOperation original;
        original.train(data.features, data.labels, nullptr);
        REQUIRE(original.isTrained());

        arma::Row<std::size_t> original_preds;
        original.predict(data.features, original_preds);

        // Save
        std::stringstream ss;
        REQUIRE(original.save(ss));

        // Load into new instance
        HiddenMarkovModelOperation loaded;
        REQUIRE(loaded.load(ss));
        CHECK(loaded.isTrained());
        CHECK(loaded.numClasses() == original.numClasses());
        CHECK(loaded.numFeatures() == original.numFeatures());

        // Predictions should match
        arma::Row<std::size_t> loaded_preds;
        loaded.predict(data.features, loaded_preds);
        CHECK(arma::approx_equal(
                arma::conv_to<arma::mat>::from(original_preds),
                arma::conv_to<arma::mat>::from(loaded_preds),
                "absdiff", 0.0));
    }

    SECTION("save fails on untrained model") {
        HiddenMarkovModelOperation untrained;
        std::stringstream ss;
        CHECK_FALSE(untrained.save(ss));
    }
}

// ============================================================================
// Registry integration
// ============================================================================

TEST_CASE("HiddenMarkovModelOperation - registry integration", "[MLCore][HMM]") {
    MLModelRegistry registry;

    SECTION("HMM is registered") {
        CHECK(registry.hasModel("Hidden Markov Model (Gaussian)"));
    }

    SECTION("HMM appears in BinaryClassification models") {
        auto names = registry.getModelNames(MLTaskType::BinaryClassification);
        bool found = false;
        for (auto const & name: names) {
            if (name == "Hidden Markov Model (Gaussian)") {
                found = true;
                break;
            }
        }
        CHECK(found);
    }

    SECTION("create returns a fresh HMM instance") {
        auto model = registry.create("Hidden Markov Model (Gaussian)");
        REQUIRE(model != nullptr);
        CHECK(model->getName() == "Hidden Markov Model (Gaussian)");
        CHECK_FALSE(model->isTrained());
        CHECK(model->isSequenceModel());
    }
}

// ============================================================================
// Base class sequence API default behavior
// ============================================================================

TEST_CASE("MLModelOperation - default trainSequences concatenates", "[MLCore][HMM]") {
    // Verify that the default trainSequences in the base class works
    // by using a non-HMM model (Random Forest)
    auto registry = MLModelRegistry{};
    auto rf = registry.create("Random Forest");
    REQUIRE(rf != nullptr);
    CHECK_FALSE(rf->isSequenceModel());

    arma::arma_rng::set_seed(42);
    arma::mat feat1 = arma::randn(2, 20);
    feat1.row(0) -= 2.0;
    feat1.row(1) -= 2.0;

    arma::mat feat2 = arma::randn(2, 20);
    feat2.row(0) += 2.0;
    feat2.row(1) += 2.0;

    arma::Row<std::size_t> lab1(20, arma::fill::zeros);
    arma::Row<std::size_t> lab2(20);
    lab2.fill(1);

    std::vector<arma::mat> seqs{feat1, feat2};
    std::vector<arma::Row<std::size_t>> labels{lab1, lab2};

    SECTION("trainSequences delegates to train for non-sequence models") {
        bool ok = rf->trainSequences(seqs, labels, nullptr);
        CHECK(ok);
        CHECK(rf->isTrained());
    }

    SECTION("predictSequences delegates to predict for non-sequence models") {
        rf->trainSequences(seqs, labels, nullptr);
        REQUIRE(rf->isTrained());

        std::vector<arma::Row<std::size_t>> predSeqs;
        bool ok = rf->predictSequences(seqs, predSeqs);
        CHECK(ok);
        REQUIRE(predSeqs.size() == 2);
        CHECK(predSeqs[0].n_elem == 20);
        CHECK(predSeqs[1].n_elem == 20);
    }
}
