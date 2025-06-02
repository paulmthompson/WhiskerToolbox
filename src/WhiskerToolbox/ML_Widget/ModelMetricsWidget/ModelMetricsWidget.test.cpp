#include "ModelMetricsWidget.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <armadillo>

TEST_CASE("ModelMetricsWidget Binary Classification Metrics - Happy Path", "[ModelMetricsWidget][BinaryClassification]") {
    
    SECTION("Perfect Classification") {
        // Arrange: Perfect predictions
        arma::Row<std::size_t> predictions = {1, 0, 1, 0, 1};
        arma::Row<std::size_t> true_labels = {1, 0, 1, 0, 1};
        
        ModelMetricsWidget widget;
        
        // Act
        widget.setBinaryClassificationMetrics(predictions, true_labels, "TestModel");
        
        // Assert
        REQUIRE(widget.hasValidMetrics());
        auto metrics = widget.getCurrentBinaryMetrics();
        
        REQUIRE_THAT(metrics.accuracy, Catch::Matchers::WithinRel(1.0, 1e-9));
        REQUIRE_THAT(metrics.sensitivity, Catch::Matchers::WithinRel(1.0, 1e-9));
        REQUIRE_THAT(metrics.specificity, Catch::Matchers::WithinRel(1.0, 1e-9));
        REQUIRE_THAT(metrics.dice_score, Catch::Matchers::WithinRel(1.0, 1e-9));
        
        REQUIRE(metrics.true_positives == 3);
        REQUIRE(metrics.true_negatives == 2);
        REQUIRE(metrics.false_positives == 0);
        REQUIRE(metrics.false_negatives == 0);
    }
    
    SECTION("Mixed Classification Results") {
        // Arrange: Mixed predictions with known confusion matrix
        arma::Row<std::size_t> predictions = {1, 0, 1, 1, 0, 0, 1, 0};
        arma::Row<std::size_t> true_labels = {1, 0, 0, 1, 0, 1, 1, 0};
        
        ModelMetricsWidget widget;
        
        // Act
        widget.setBinaryClassificationMetrics(predictions, true_labels, "TestModel");
        
        // Assert
        REQUIRE(widget.hasValidMetrics());
        auto metrics = widget.getCurrentBinaryMetrics();
        
        // Expected confusion matrix:
        // TP = 3 (predictions[0,3,6] = 1, true_labels[0,3,6] = 1)
        // TN = 3 (predictions[1,4,7] = 0, true_labels[1,4,7] = 0)
        // FP = 1 (predictions[2] = 1, true_labels[2] = 0)
        // FN = 1 (predictions[5] = 0, true_labels[5] = 1)
        
        REQUIRE(metrics.true_positives == 3);
        REQUIRE(metrics.true_negatives == 3);
        REQUIRE(metrics.false_positives == 1);
        REQUIRE(metrics.false_negatives == 1);
        
        // Expected metrics:
        // Accuracy = (TP + TN) / Total = (3 + 3) / 8 = 0.75
        // Sensitivity = TP / (TP + FN) = 3 / (3 + 1) = 0.75
        // Specificity = TN / (TN + FP) = 3 / (3 + 1) = 0.75
        // Dice Score = 2*TP / (2*TP + FP + FN) = 6 / (6 + 1 + 1) = 0.75
        
        REQUIRE_THAT(metrics.accuracy, Catch::Matchers::WithinRel(0.75, 1e-9));
        REQUIRE_THAT(metrics.sensitivity, Catch::Matchers::WithinRel(0.75, 1e-9));
        REQUIRE_THAT(metrics.specificity, Catch::Matchers::WithinRel(0.75, 1e-9));
        REQUIRE_THAT(metrics.dice_score, Catch::Matchers::WithinRel(0.75, 1e-9));
    }
    
    SECTION("All Positive Predictions") {
        // Arrange: All predictions are positive
        arma::Row<std::size_t> predictions = {1, 1, 1, 1};
        arma::Row<std::size_t> true_labels = {1, 0, 1, 0};
        
        ModelMetricsWidget widget;
        
        // Act
        widget.setBinaryClassificationMetrics(predictions, true_labels, "TestModel");
        
        // Assert
        auto metrics = widget.getCurrentBinaryMetrics();
        
        REQUIRE(metrics.true_positives == 2);
        REQUIRE(metrics.true_negatives == 0);
        REQUIRE(metrics.false_positives == 2);
        REQUIRE(metrics.false_negatives == 0);
        
        // Accuracy = (2 + 0) / 4 = 0.5
        // Sensitivity = 2 / (2 + 0) = 1.0
        // Specificity = 0 / (0 + 2) = 0.0
        // Dice Score = 4 / (4 + 2 + 0) = 0.667
        
        REQUIRE_THAT(metrics.accuracy, Catch::Matchers::WithinRel(0.5, 1e-9));
        REQUIRE_THAT(metrics.sensitivity, Catch::Matchers::WithinRel(1.0, 1e-9));
        REQUIRE_THAT(metrics.specificity, Catch::Matchers::WithinRel(0.0, 1e-9));
        REQUIRE_THAT(metrics.dice_score, Catch::Matchers::WithinRel(2.0/3.0, 1e-9));
    }
}

TEST_CASE("ModelMetricsWidget Binary Classification Metrics - Error Cases and Edge Cases", "[ModelMetricsWidget][BinaryClassification][ErrorHandling]") {
    
    SECTION("Empty Vectors") {
        arma::Row<std::size_t> predictions;
        arma::Row<std::size_t> true_labels;
        
        ModelMetricsWidget widget;
        widget.setBinaryClassificationMetrics(predictions, true_labels, "TestModel");
        
        // Should show no metrics state
        REQUIRE_FALSE(widget.hasValidMetrics());
    }
    
    SECTION("Mismatched Vector Sizes") {
        arma::Row<std::size_t> predictions = {1, 0, 1};
        arma::Row<std::size_t> true_labels = {1, 0};
        
        ModelMetricsWidget widget;
        widget.setBinaryClassificationMetrics(predictions, true_labels, "TestModel");
        
        // Should show no metrics state
        REQUIRE_FALSE(widget.hasValidMetrics());
    }
    
    SECTION("All Zeros") {
        arma::Row<std::size_t> predictions = {0, 0, 0, 0};
        arma::Row<std::size_t> true_labels = {0, 0, 0, 0};
        
        ModelMetricsWidget widget;
        widget.setBinaryClassificationMetrics(predictions, true_labels, "TestModel");
        
        auto metrics = widget.getCurrentBinaryMetrics();
        
        REQUIRE(metrics.true_positives == 0);
        REQUIRE(metrics.true_negatives == 4);
        REQUIRE(metrics.false_positives == 0);
        REQUIRE(metrics.false_negatives == 0);
        
        // Accuracy = 4/4 = 1.0
        // Sensitivity = 0/(0+0) = undefined, should be 0
        // Specificity = 4/(4+0) = 1.0
        // Dice Score = 0/(0+0+0) = undefined, should be 0
        
        REQUIRE_THAT(metrics.accuracy, Catch::Matchers::WithinRel(1.0, 1e-9));
        REQUIRE_THAT(metrics.sensitivity, Catch::Matchers::WithinRel(0.0, 1e-9));
        REQUIRE_THAT(metrics.specificity, Catch::Matchers::WithinRel(1.0, 1e-9));
        REQUIRE_THAT(metrics.dice_score, Catch::Matchers::WithinRel(0.0, 1e-9));
    }
    
    SECTION("Clear Metrics") {
        ModelMetricsWidget widget;
        
        // Set some metrics first
        arma::Row<std::size_t> predictions = {1, 0};
        arma::Row<std::size_t> true_labels = {1, 0};
        widget.setBinaryClassificationMetrics(predictions, true_labels, "TestModel");
        
        REQUIRE(widget.hasValidMetrics());
        
        // Clear metrics
        widget.clearMetrics();
        
        REQUIRE_FALSE(widget.hasValidMetrics());
    }
    
    SECTION("Invalid Metrics Struct") {
        BinaryClassificationMetrics invalid_metrics; // Default constructor creates invalid metrics
        
        ModelMetricsWidget widget;
        widget.setBinaryClassificationMetrics(invalid_metrics, "TestModel");
        
        REQUIRE_FALSE(widget.hasValidMetrics());
    }
} 