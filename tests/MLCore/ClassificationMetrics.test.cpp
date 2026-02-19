
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "metrics/ClassificationMetrics.hpp"

#include <armadillo>
#include <cstddef>
#include <string>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

// ============================================================================
// Binary classification — happy path
// ============================================================================

TEST_CASE("BinaryMetrics: perfect classification", "[ClassificationMetrics][Binary]") {
    arma::Row<std::size_t> predictions = {1, 0, 1, 0, 1};
    arma::Row<std::size_t> truth       = {1, 0, 1, 0, 1};

    auto m = MLCore::computeBinaryMetrics(predictions, truth);

    REQUIRE(m.isValid());
    REQUIRE(m.getTotalPredictions() == 5);

    REQUIRE(m.true_positives  == 3);
    REQUIRE(m.true_negatives  == 2);
    REQUIRE(m.false_positives == 0);
    REQUIRE(m.false_negatives == 0);

    REQUIRE_THAT(m.accuracy,    WithinRel(1.0, 1e-9));
    REQUIRE_THAT(m.sensitivity, WithinRel(1.0, 1e-9));
    REQUIRE_THAT(m.specificity, WithinRel(1.0, 1e-9));
    REQUIRE_THAT(m.dice_score,  WithinRel(1.0, 1e-9));
}

TEST_CASE("BinaryMetrics: mixed results", "[ClassificationMetrics][Binary]") {
    // pred:  1 0 1 1 0 0 1 0
    // truth: 1 0 0 1 0 1 1 0
    // TP=3 TN=3 FP=1 FN=1
    arma::Row<std::size_t> predictions = {1, 0, 1, 1, 0, 0, 1, 0};
    arma::Row<std::size_t> truth       = {1, 0, 0, 1, 0, 1, 1, 0};

    auto m = MLCore::computeBinaryMetrics(predictions, truth);

    REQUIRE(m.true_positives  == 3);
    REQUIRE(m.true_negatives  == 3);
    REQUIRE(m.false_positives == 1);
    REQUIRE(m.false_negatives == 1);

    REQUIRE_THAT(m.accuracy,    WithinRel(0.75, 1e-9));
    REQUIRE_THAT(m.sensitivity, WithinRel(0.75, 1e-9));
    REQUIRE_THAT(m.specificity, WithinRel(0.75, 1e-9));
    REQUIRE_THAT(m.dice_score,  WithinRel(0.75, 1e-9));
}

TEST_CASE("BinaryMetrics: all positive predictions", "[ClassificationMetrics][Binary]") {
    arma::Row<std::size_t> predictions = {1, 1, 1, 1};
    arma::Row<std::size_t> truth       = {1, 0, 1, 0};

    auto m = MLCore::computeBinaryMetrics(predictions, truth);

    REQUIRE(m.true_positives  == 2);
    REQUIRE(m.true_negatives  == 0);
    REQUIRE(m.false_positives == 2);
    REQUIRE(m.false_negatives == 0);

    REQUIRE_THAT(m.accuracy,    WithinRel(0.5, 1e-9));
    REQUIRE_THAT(m.sensitivity, WithinRel(1.0, 1e-9));
    REQUIRE_THAT(m.specificity, WithinAbs(0.0, 1e-12));
    REQUIRE_THAT(m.dice_score,  WithinRel(2.0 / 3.0, 1e-9));
}

TEST_CASE("BinaryMetrics: all negative predictions", "[ClassificationMetrics][Binary]") {
    arma::Row<std::size_t> predictions = {0, 0, 0, 0};
    arma::Row<std::size_t> truth       = {1, 0, 1, 0};

    auto m = MLCore::computeBinaryMetrics(predictions, truth);

    REQUIRE(m.true_positives  == 0);
    REQUIRE(m.true_negatives  == 2);
    REQUIRE(m.false_positives == 0);
    REQUIRE(m.false_negatives == 2);

    REQUIRE_THAT(m.accuracy,    WithinRel(0.5, 1e-9));
    REQUIRE_THAT(m.sensitivity, WithinAbs(0.0, 1e-12));
    REQUIRE_THAT(m.specificity, WithinRel(1.0, 1e-9));
    REQUIRE_THAT(m.dice_score,  WithinAbs(0.0, 1e-12));
}

TEST_CASE("BinaryMetrics: all zeros (all TN)", "[ClassificationMetrics][Binary]") {
    arma::Row<std::size_t> predictions = {0, 0, 0, 0};
    arma::Row<std::size_t> truth       = {0, 0, 0, 0};

    auto m = MLCore::computeBinaryMetrics(predictions, truth);

    REQUIRE(m.true_positives  == 0);
    REQUIRE(m.true_negatives  == 4);
    REQUIRE(m.false_positives == 0);
    REQUIRE(m.false_negatives == 0);

    REQUIRE_THAT(m.accuracy,    WithinRel(1.0, 1e-9));
    REQUIRE_THAT(m.sensitivity, WithinAbs(0.0, 1e-12));   // 0/0 → 0
    REQUIRE_THAT(m.specificity, WithinRel(1.0, 1e-9));
    REQUIRE_THAT(m.dice_score,  WithinAbs(0.0, 1e-12));   // 0/0 → 0
}

// ============================================================================
// Binary classification — edge cases / errors
// ============================================================================

TEST_CASE("BinaryMetrics: empty vectors return invalid", "[ClassificationMetrics][Binary]") {
    arma::Row<std::size_t> predictions;
    arma::Row<std::size_t> truth;

    auto m = MLCore::computeBinaryMetrics(predictions, truth);

    REQUIRE_FALSE(m.isValid());
    REQUIRE(m.getTotalPredictions() == 0);
}

TEST_CASE("BinaryMetrics: mismatched sizes throw", "[ClassificationMetrics][Binary]") {
    arma::Row<std::size_t> predictions = {1, 0, 1};
    arma::Row<std::size_t> truth       = {1, 0};

    REQUIRE_THROWS_AS(
        MLCore::computeBinaryMetrics(predictions, truth),
        std::invalid_argument);
}

TEST_CASE("BinaryMetrics: single element", "[ClassificationMetrics][Binary]") {
    SECTION("TP only") {
        arma::Row<std::size_t> p = {1};
        arma::Row<std::size_t> t = {1};
        auto m = MLCore::computeBinaryMetrics(p, t);
        REQUIRE(m.true_positives == 1);
        REQUIRE_THAT(m.accuracy, WithinRel(1.0, 1e-9));
    }
    SECTION("FP only") {
        arma::Row<std::size_t> p = {1};
        arma::Row<std::size_t> t = {0};
        auto m = MLCore::computeBinaryMetrics(p, t);
        REQUIRE(m.false_positives == 1);
        REQUIRE_THAT(m.accuracy, WithinAbs(0.0, 1e-12));
    }
}

TEST_CASE("BinaryMetrics: non-binary values are skipped", "[ClassificationMetrics][Binary]") {
    // values > 1 should be silently ignored
    arma::Row<std::size_t> predictions = {1, 0, 5, 1};
    arma::Row<std::size_t> truth       = {1, 0, 1, 0};

    auto m = MLCore::computeBinaryMetrics(predictions, truth);

    // Only 3 valid entries (indices 0, 1, 3); index 2 has pred=5 → skipped
    REQUIRE(m.getTotalPredictions() == 3);
    REQUIRE(m.true_positives  == 1);
    REQUIRE(m.true_negatives  == 1);
    REQUIRE(m.false_positives == 1);
    REQUIRE(m.false_negatives == 0);
}

TEST_CASE("BinaryMetrics: isValid and getTotalPredictions", "[ClassificationMetrics][Binary]") {
    MLCore::BinaryClassificationMetrics m;
    REQUIRE_FALSE(m.isValid());
    REQUIRE(m.getTotalPredictions() == 0);

    m.true_positives = 1;
    REQUIRE(m.isValid());
    REQUIRE(m.getTotalPredictions() == 1);
}

// ============================================================================
// Multi-class classification — happy path
// ============================================================================

TEST_CASE("MultiClassMetrics: perfect 3-class classification", "[ClassificationMetrics][MultiClass]") {
    arma::Row<std::size_t> predictions = {0, 1, 2, 0, 1, 2};
    arma::Row<std::size_t> truth       = {0, 1, 2, 0, 1, 2};

    auto m = MLCore::computeMultiClassMetrics(predictions, truth, 3);

    REQUIRE(m.isValid());
    REQUIRE(m.num_classes == 3);
    REQUIRE_THAT(m.overall_accuracy, WithinRel(1.0, 1e-9));

    // Confusion matrix is diagonal
    for (std::size_t r = 0; r < 3; ++r) {
        for (std::size_t c = 0; c < 3; ++c) {
            if (r == c) {
                REQUIRE(m.confusion_matrix(r, c) == 2);
            } else {
                REQUIRE(m.confusion_matrix(r, c) == 0);
            }
        }
    }

    for (std::size_t c = 0; c < 3; ++c) {
        REQUIRE_THAT(m.per_class_precision[c], WithinRel(1.0, 1e-9));
        REQUIRE_THAT(m.per_class_recall[c],    WithinRel(1.0, 1e-9));
        REQUIRE_THAT(m.per_class_f1[c],        WithinRel(1.0, 1e-9));
    }
}

TEST_CASE("MultiClassMetrics: some misclassifications", "[ClassificationMetrics][MultiClass]") {
    // 3 classes, 6 observations
    // truth: 0 0 1 1 2 2
    // pred:  0 1 1 2 2 0
    // Confusion (truth=rows, pred=cols):
    //        pred0  pred1  pred2
    // true0:   1      1      0
    // true1:   0      1      1
    // true2:   1      0      1
    arma::Row<std::size_t> predictions = {0, 1, 1, 2, 2, 0};
    arma::Row<std::size_t> truth       = {0, 0, 1, 1, 2, 2};

    auto m = MLCore::computeMultiClassMetrics(predictions, truth, 3);

    REQUIRE(m.num_classes == 3);
    REQUIRE_THAT(m.overall_accuracy, WithinRel(3.0 / 6.0, 1e-9));

    REQUIRE(m.confusion_matrix(0, 0) == 1);
    REQUIRE(m.confusion_matrix(0, 1) == 1);
    REQUIRE(m.confusion_matrix(0, 2) == 0);
    REQUIRE(m.confusion_matrix(1, 0) == 0);
    REQUIRE(m.confusion_matrix(1, 1) == 1);
    REQUIRE(m.confusion_matrix(1, 2) == 1);
    REQUIRE(m.confusion_matrix(2, 0) == 1);
    REQUIRE(m.confusion_matrix(2, 1) == 0);
    REQUIRE(m.confusion_matrix(2, 2) == 1);

    // Class 0: TP=1, FP=1 (from true2→pred0), FN=1 (true0→pred1)
    // precision = 1/2 = 0.5, recall = 1/2 = 0.5, F1 = 0.5
    REQUIRE_THAT(m.per_class_precision[0], WithinRel(0.5, 1e-9));
    REQUIRE_THAT(m.per_class_recall[0],    WithinRel(0.5, 1e-9));
    REQUIRE_THAT(m.per_class_f1[0],        WithinRel(0.5, 1e-9));
}

TEST_CASE("MultiClassMetrics: inferred num_classes", "[ClassificationMetrics][MultiClass]") {
    arma::Row<std::size_t> predictions = {0, 2, 1};
    arma::Row<std::size_t> truth       = {0, 2, 2};

    auto m = MLCore::computeMultiClassMetrics(predictions, truth);  // no num_classes arg

    REQUIRE(m.num_classes == 3);
    REQUIRE(m.isValid());
}

// ============================================================================
// Multi-class — edge cases / errors
// ============================================================================

TEST_CASE("MultiClassMetrics: mismatched sizes throw", "[ClassificationMetrics][MultiClass]") {
    arma::Row<std::size_t> predictions = {0, 1};
    arma::Row<std::size_t> truth       = {0};

    REQUIRE_THROWS_AS(
        MLCore::computeMultiClassMetrics(predictions, truth, 2),
        std::invalid_argument);
}

TEST_CASE("MultiClassMetrics: zero num_classes throws", "[ClassificationMetrics][MultiClass]") {
    arma::Row<std::size_t> predictions = {0};
    arma::Row<std::size_t> truth       = {0};

    REQUIRE_THROWS_AS(
        MLCore::computeMultiClassMetrics(predictions, truth, 0),
        std::invalid_argument);
}

TEST_CASE("MultiClassMetrics: empty vectors with explicit num_classes", "[ClassificationMetrics][MultiClass]") {
    arma::Row<std::size_t> predictions;
    arma::Row<std::size_t> truth;

    auto m = MLCore::computeMultiClassMetrics(predictions, truth, 3);
    // num_classes is set but confusion_matrix is empty → isValid returns false
    REQUIRE_FALSE(m.isValid());
}

TEST_CASE("MultiClassMetrics: empty vectors for inferred overload throws", "[ClassificationMetrics][MultiClass]") {
    arma::Row<std::size_t> predictions;
    arma::Row<std::size_t> truth;

    REQUIRE_THROWS_AS(
        MLCore::computeMultiClassMetrics(predictions, truth),
        std::invalid_argument);
}

TEST_CASE("MultiClassMetrics: labels exceeding num_classes are ignored safely", "[ClassificationMetrics][MultiClass]") {
    // label 5 exceeds num_classes=3 → those entries are simply not counted
    arma::Row<std::size_t> predictions = {0, 5, 1};
    arma::Row<std::size_t> truth       = {0, 1, 1};

    auto m = MLCore::computeMultiClassMetrics(predictions, truth, 3);

    // Only entries 0 and 2 are valid (within [0,3))
    // Entry 1: pred=5 >= 3 → skipped
    // Accuracy = 2 correct out of 3 total? No — the function counts correct only
    // among valid entries but divides by total n_elem. Entry 1 contributes 0 to
    // correct but still counts in denominator.
    REQUIRE_THAT(m.overall_accuracy, WithinRel(2.0 / 3.0, 1e-9));
}

// ============================================================================
// Binary metrics can also be computed via 2-class multi-class
// ============================================================================

TEST_CASE("MultiClassMetrics: 2-class matches binary", "[ClassificationMetrics]") {
    arma::Row<std::size_t> predictions = {1, 0, 1, 1, 0, 0, 1, 0};
    arma::Row<std::size_t> truth       = {1, 0, 0, 1, 0, 1, 1, 0};

    auto binary = MLCore::computeBinaryMetrics(predictions, truth);
    auto multi  = MLCore::computeMultiClassMetrics(predictions, truth, 2);

    // Overall accuracy should match
    REQUIRE_THAT(multi.overall_accuracy, WithinRel(binary.accuracy, 1e-9));

    // Class 1 recall = sensitivity, class 0 recall = specificity
    REQUIRE_THAT(multi.per_class_recall[1], WithinRel(binary.sensitivity, 1e-9));
    REQUIRE_THAT(multi.per_class_recall[0], WithinRel(binary.specificity, 1e-9));
}

// ============================================================================
// Formatting
// ============================================================================

TEST_CASE("formatBinaryMetrics produces non-empty string", "[ClassificationMetrics][Format]") {
    arma::Row<std::size_t> predictions = {1, 0, 1};
    arma::Row<std::size_t> truth       = {1, 0, 0};

    auto m = MLCore::computeBinaryMetrics(predictions, truth);
    auto s = MLCore::formatBinaryMetrics(m);

    REQUIRE(!s.empty());
    REQUIRE(s.find("Accuracy") != std::string::npos);
    REQUIRE(s.find("TP") != std::string::npos);
}

TEST_CASE("formatMultiClassMetrics produces non-empty string", "[ClassificationMetrics][Format]") {
    arma::Row<std::size_t> predictions = {0, 1, 2};
    arma::Row<std::size_t> truth       = {0, 1, 2};

    auto m = MLCore::computeMultiClassMetrics(predictions, truth, 3);
    auto s = MLCore::formatMultiClassMetrics(m, {"Cat", "Dog", "Bird"});

    REQUIRE(!s.empty());
    REQUIRE(s.find("Cat") != std::string::npos);
    REQUIRE(s.find("Dog") != std::string::npos);
    REQUIRE(s.find("Confusion") != std::string::npos);
}

TEST_CASE("formatMultiClassMetrics works without class names", "[ClassificationMetrics][Format]") {
    arma::Row<std::size_t> predictions = {0, 1};
    arma::Row<std::size_t> truth       = {0, 1};

    auto m = MLCore::computeMultiClassMetrics(predictions, truth, 2);
    auto s = MLCore::formatMultiClassMetrics(m);

    REQUIRE(!s.empty());
    REQUIRE(s.find("0") != std::string::npos);
}
