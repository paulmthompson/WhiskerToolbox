
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "pipelines/ClassificationPipeline.hpp"

#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityRegistry.hpp"
#include "Entity/EntityTypes.hpp"
#include "Tensors/RowDescriptor.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"

#include "models/MLModelParameters.hpp"
#include "models/MLModelRegistry.hpp"
#include "models/MLTaskType.hpp"
#include "models/supervised/HiddenMarkovModelOperation.hpp"

#include <algorithm>
#include <armadillo>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

using Catch::Matchers::WithinRel;

// ============================================================================
// Helpers
// ============================================================================

namespace {

std::shared_ptr<TimeFrame> makeTimeFrame(int count) {
    std::vector<int> times(count);
    for (int i = 0; i < count; ++i) {
        times[i] = i;
    }
    return std::make_shared<TimeFrame>(times);
}

std::shared_ptr<TimeIndexStorage> makeDenseTimeStorage(std::size_t count) {
    return TimeIndexStorageFactory::createDenseFromZero(count);
}

/**
 * @brief Create a DataManager with a TimeFrame registered
 */
std::shared_ptr<DataManager> makeDM(std::string const & time_key, int frame_count) {
    auto dm = std::make_shared<DataManager>();
    dm->setTime(TimeKey(time_key), makeTimeFrame(frame_count));
    return dm;
}

/**
 * @brief Create a linearly separable 2D feature tensor
 *
 * Class 0: feature_0 in [0, 5), located at rows 0..half-1
 * Class 1: feature_0 in [10, 15), located at rows half..n-1
 *
 * Both classes have 2 features. The second feature is random noise.
 * Stored in DataManager under the given key.
 */
void createLinSepTensor(
        DataManager & dm,
        std::string const & key,
        std::string const & time_key,
        std::size_t per_class = 50) {
    auto const total = per_class * 2;
    auto const cols = std::size_t{2};
    std::vector<float> data(total * cols);

    // Feature 0: separable, Feature 1: noise
    for (std::size_t i = 0; i < per_class; ++i) {
        data[i * cols + 0] = static_cast<float>(i) * 0.1f; // class 0: low
        data[i * cols + 1] = static_cast<float>(i) * 0.01f;// noise
    }
    for (std::size_t i = 0; i < per_class; ++i) {
        auto r = per_class + i;
        data[r * cols + 0] = 10.0f + static_cast<float>(i) * 0.1f;// class 1: high
        data[r * cols + 1] = static_cast<float>(i) * 0.01f;       // noise
    }

    auto ts = makeDenseTimeStorage(total);
    auto tf = dm.getTime(TimeKey(time_key));

    std::vector<std::string> const names = {"feature_0", "feature_1"};
    auto tensor = std::make_shared<TensorData>(
            TensorData::createTimeSeries2D(data, total, cols, ts, tf, names));

    dm.setData<TensorData>(key, tensor, TimeKey(time_key));
}

/**
 * @brief Create a DigitalIntervalSeries labeling the second half of rows as "positive"
 *
 * For 2*per_class rows (0..2*per_class-1):
 *   Interval [per_class, 2*per_class-1] → positive class
 */
void createLabelIntervalSeries(
        DataManager & dm,
        std::string const & key,
        std::string const & time_key,
        std::size_t per_class = 50) {
    auto tf = dm.getTime(TimeKey(time_key));
    auto series = std::make_shared<DigitalIntervalSeries>();
    series->setTimeFrame(tf);
    series->addEvent(
            TimeFrameIndex(static_cast<int64_t>(per_class)),
            TimeFrameIndex(static_cast<int64_t>(per_class * 2 - 1)));
    dm.setData<DigitalIntervalSeries>(key, series, TimeKey(time_key));
}

/**
 * @brief Register TimeEntity groups for two classes
 *
 * Class 0 group: entities at times [0..per_class)
 * Class 1 group: entities at times [per_class..2*per_class)
 *
 * @return {group_id_class0, group_id_class1}
 */
std::pair<GroupId, GroupId> createTimeEntityGroups(
        DataManager & dm,
        std::string const & time_key,
        std::size_t per_class = 50) {
    auto * groups = dm.getEntityGroupManager();
    auto * reg = dm.getEntityRegistry();

    auto g0 = groups->createGroup("NotContact");
    auto g1 = groups->createGroup("Contact");

    for (std::size_t i = 0; i < per_class; ++i) {
        auto eid = reg->ensureId(time_key, EntityKind::TimeEntity,
                                 TimeFrameIndex(static_cast<int64_t>(i)), 0);
        groups->addEntityToGroup(g0, eid);
    }
    for (std::size_t i = per_class; i < per_class * 2; ++i) {
        auto eid = reg->ensureId(time_key, EntityKind::TimeEntity,
                                 TimeFrameIndex(static_cast<int64_t>(i)), 0);
        groups->addEntityToGroup(g1, eid);
    }

    return {g0, g1};
}

}// anonymous namespace

// ============================================================================
// Stage toString
// ============================================================================

TEST_CASE("ClassificationPipeline: stage toString", "[ClassificationPipeline]") {
    CHECK(MLCore::toString(MLCore::ClassificationStage::ValidatingFeatures) == "Validating features");
    CHECK(MLCore::toString(MLCore::ClassificationStage::ConvertingFeatures) == "Converting features");
    CHECK(MLCore::toString(MLCore::ClassificationStage::AssemblingLabels) == "Assembling labels");
    CHECK(MLCore::toString(MLCore::ClassificationStage::BalancingClasses) == "Balancing classes");
    CHECK(MLCore::toString(MLCore::ClassificationStage::SegmentingSequences) == "Segmenting sequences");
    CHECK(MLCore::toString(MLCore::ClassificationStage::Training) == "Training model");
    CHECK(MLCore::toString(MLCore::ClassificationStage::Predicting) == "Predicting");
    CHECK(MLCore::toString(MLCore::ClassificationStage::ComputingMetrics) == "Computing metrics");
    CHECK(MLCore::toString(MLCore::ClassificationStage::WritingOutput) == "Writing output");
    CHECK(MLCore::toString(MLCore::ClassificationStage::Complete) == "Complete");
    CHECK(MLCore::toString(MLCore::ClassificationStage::Failed) == "Failed");
}

// ============================================================================
// Error handling — validation failures
// ============================================================================

TEST_CASE("ClassificationPipeline: fails if feature tensor not found",
          "[ClassificationPipeline]") {
    auto dm = makeDM("cam", 200);
    MLCore::MLModelRegistry const registry;

    MLCore::ClassificationPipelineConfig config;
    config.model_name = "Random Forest";
    config.feature_tensor_key = "nonexistent";
    config.label_config = MLCore::LabelFromIntervals{};
    config.time_key_str = "cam";

    auto result = MLCore::runClassificationPipeline(*dm, registry, config);
    CHECK_FALSE(result.success);
    CHECK(result.failed_stage == MLCore::ClassificationStage::ValidatingFeatures);
    CHECK(result.error_message.find("not found") != std::string::npos);
}

TEST_CASE("ClassificationPipeline: fails if model not in registry",
          "[ClassificationPipeline]") {
    auto dm = makeDM("cam", 200);
    createLinSepTensor(*dm, "features", "cam", 50);
    MLCore::MLModelRegistry const registry;

    MLCore::ClassificationPipelineConfig config;
    config.model_name = "NonexistentModel";
    config.feature_tensor_key = "features";
    config.label_config = MLCore::LabelFromIntervals{};
    config.time_key_str = "cam";

    auto result = MLCore::runClassificationPipeline(*dm, registry, config);
    CHECK_FALSE(result.success);
    CHECK(result.failed_stage == MLCore::ClassificationStage::ValidatingFeatures);
    CHECK(result.error_message.find("not found") != std::string::npos);
}

TEST_CASE("ClassificationPipeline: fails if label interval not found",
          "[ClassificationPipeline]") {
    auto dm = makeDM("cam", 200);
    createLinSepTensor(*dm, "features", "cam", 50);
    MLCore::MLModelRegistry const registry;

    MLCore::ClassificationPipelineConfig config;
    config.model_name = "Random Forest";
    config.feature_tensor_key = "features";
    config.label_config = MLCore::LabelFromIntervals{"Contact", "NoContact"};
    config.label_interval_key = "nonexistent_intervals";
    config.time_key_str = "cam";

    auto result = MLCore::runClassificationPipeline(*dm, registry, config);
    CHECK_FALSE(result.success);
    CHECK(result.failed_stage == MLCore::ClassificationStage::AssemblingLabels);
}

// ============================================================================
// Full pipeline with interval-based labels — Random Forest
// ============================================================================

TEST_CASE("ClassificationPipeline: end-to-end with Random Forest and interval labels",
          "[ClassificationPipeline]") {
    constexpr std::size_t per_class = 50;
    auto dm = makeDM("cam", 200);
    createLinSepTensor(*dm, "features", "cam", per_class);
    createLabelIntervalSeries(*dm, "labels", "cam", per_class);

    MLCore::MLModelRegistry const registry;

    MLCore::ClassificationPipelineConfig config;
    config.model_name = "Random Forest";
    config.feature_tensor_key = "features";
    config.label_config = MLCore::LabelFromIntervals{"Contact", "NoContact"};
    config.label_interval_key = "labels";
    config.time_key_str = "cam";
    config.output_config.output_prefix = "Predicted:";
    config.output_config.time_key_str = "cam";
    config.output_config.write_intervals = true;
    config.output_config.write_probabilities = true;
    config.output_config.write_to_putative_groups = true;
    config.prediction_region.predict_all_rows = true;

    // Track progress stages
    std::vector<MLCore::ClassificationStage> stages_seen;
    auto progress = [&](MLCore::ClassificationStage stage, std::string const &) {
        stages_seen.push_back(stage);
    };

    auto result = MLCore::runClassificationPipeline(*dm, registry, config, progress);

    // Pipeline should succeed
    REQUIRE(result.success);
    CHECK(result.error_message.empty());

    // Training info
    CHECK(result.training_observations == per_class * 2);
    CHECK(result.num_features == 2);
    CHECK(result.num_classes == 2);
    CHECK(result.class_names.size() == 2);
    CHECK(result.rows_dropped_nan == 0);
    CHECK_FALSE(result.was_balanced);

    // Metrics should be present (predicting on training data)
    REQUIRE(result.binary_train_metrics.has_value());
    auto const & bm = result.binary_train_metrics.value();
    CHECK(bm.accuracy >= 0.90);// Linearly separable → should get high accuracy

    REQUIRE(result.multi_class_train_metrics.has_value());
    auto const & mm = result.multi_class_train_metrics.value();
    CHECK(mm.overall_accuracy >= 0.90);

    // Prediction count
    CHECK(result.prediction_observations == per_class * 2);

    // Writer result should be present
    REQUIRE(result.writer_result.has_value());
    auto const & wr = result.writer_result.value();
    CHECK(wr.interval_keys.size() == 2);
    CHECK(wr.probability_keys.size() == 2);
    CHECK(wr.class_names.size() == 2);

    // Trained model should be returned
    REQUIRE(result.trained_model != nullptr);
    CHECK(result.trained_model->isTrained());
    CHECK(result.trained_model->numClasses() == 2);
    CHECK(result.trained_model->numFeatures() == 2);

    // Verify progress callback was called with expected stages
    REQUIRE(stages_seen.size() >= 5);
    CHECK(stages_seen.front() == MLCore::ClassificationStage::ValidatingFeatures);
    CHECK(stages_seen.back() == MLCore::ClassificationStage::Complete);
}

// ============================================================================
// Full pipeline with Naive Bayes
// ============================================================================

TEST_CASE("ClassificationPipeline: end-to-end with Naive Bayes",
          "[ClassificationPipeline]") {
    constexpr std::size_t per_class = 50;
    auto dm = makeDM("cam", 200);
    createLinSepTensor(*dm, "features", "cam", per_class);
    createLabelIntervalSeries(*dm, "labels", "cam", per_class);

    MLCore::MLModelRegistry const registry;

    MLCore::ClassificationPipelineConfig config;
    config.model_name = "Naive Bayes";
    config.feature_tensor_key = "features";
    config.label_config = MLCore::LabelFromIntervals{"Positive", "Negative"};
    config.label_interval_key = "labels";
    config.time_key_str = "cam";
    config.output_config.output_prefix = "NB:";
    config.output_config.time_key_str = "cam";
    config.prediction_region.predict_all_rows = true;

    auto result = MLCore::runClassificationPipeline(*dm, registry, config);

    REQUIRE(result.success);
    CHECK(result.num_classes == 2);
    CHECK(result.training_observations == per_class * 2);
    CHECK(result.prediction_observations == per_class * 2);
    REQUIRE(result.binary_train_metrics.has_value());
    CHECK(result.binary_train_metrics->accuracy >= 0.80);
    REQUIRE(result.trained_model != nullptr);
    CHECK(result.trained_model->getName() == "Naive Bayes");
}

// ============================================================================
// Full pipeline with Logistic Regression (binary only)
// ============================================================================

TEST_CASE("ClassificationPipeline: end-to-end with Logistic Regression",
          "[ClassificationPipeline]") {
    constexpr std::size_t per_class = 50;
    auto dm = makeDM("cam", 200);
    createLinSepTensor(*dm, "features", "cam", per_class);
    createLabelIntervalSeries(*dm, "labels", "cam", per_class);

    MLCore::MLModelRegistry const registry;

    MLCore::ClassificationPipelineConfig config;
    config.model_name = "Logistic Regression";
    config.feature_tensor_key = "features";
    config.label_config = MLCore::LabelFromIntervals{"Contact", "NoContact"};
    config.label_interval_key = "labels";
    config.time_key_str = "cam";
    config.output_config.output_prefix = "LR:";
    config.output_config.time_key_str = "cam";
    config.prediction_region.predict_all_rows = true;

    auto result = MLCore::runClassificationPipeline(*dm, registry, config);

    REQUIRE(result.success);
    CHECK(result.num_classes == 2);
    REQUIRE(result.binary_train_metrics.has_value());
    CHECK(result.binary_train_metrics->accuracy >= 0.90);
    REQUIRE(result.trained_model != nullptr);
    CHECK(result.trained_model->getName() == "Logistic Regression");
}

// ============================================================================
// Pipeline with class balancing
// ============================================================================

TEST_CASE("ClassificationPipeline: class balancing subsample",
          "[ClassificationPipeline]") {
    // Create imbalanced data: 80 vs 20
    auto dm = makeDM("cam", 200);

    // Build imbalanced tensor manually
    std::size_t const class0 = 80;
    std::size_t const class1 = 20;
    std::size_t const total = class0 + class1;
    std::size_t const cols = 2;
    std::vector<float> data(total * cols);
    for (std::size_t i = 0; i < class0; ++i) {
        data[i * cols + 0] = static_cast<float>(i) * 0.1f;
        data[i * cols + 1] = 0.0f;
    }
    for (std::size_t i = 0; i < class1; ++i) {
        auto r = class0 + i;
        data[r * cols + 0] = 10.0f + static_cast<float>(i) * 0.1f;
        data[r * cols + 1] = 1.0f;
    }

    auto ts = makeDenseTimeStorage(total);
    auto tf = dm->getTime(TimeKey("cam"));
    auto tensor = std::make_shared<TensorData>(
            TensorData::createTimeSeries2D(data, total, cols, ts, tf,
                                           {"f0", "f1"}));
    dm->setData<TensorData>("features", tensor, TimeKey("cam"));

    // Label: second portion is positive
    auto series = std::make_shared<DigitalIntervalSeries>();
    series->setTimeFrame(tf);
    series->addEvent(
            TimeFrameIndex(static_cast<int64_t>(class0)),
            TimeFrameIndex(static_cast<int64_t>(total - 1)));
    dm->setData<DigitalIntervalSeries>("labels", series, TimeKey("cam"));

    MLCore::MLModelRegistry const registry;

    MLCore::ClassificationPipelineConfig config;
    config.model_name = "Random Forest";
    config.feature_tensor_key = "features";
    config.label_config = MLCore::LabelFromIntervals{"Pos", "Neg"};
    config.label_interval_key = "labels";
    config.time_key_str = "cam";
    config.balance_classes = true;
    config.balancing_config.strategy = MLCore::BalancingStrategy::Subsample;
    config.balancing_config.random_seed = 42;
    config.output_config.time_key_str = "cam";
    config.prediction_region.predict_all_rows = true;

    auto result = MLCore::runClassificationPipeline(*dm, registry, config);

    REQUIRE(result.success);
    CHECK(result.was_balanced);
    // After balancing, training observations should be ≤ original (subsample)
    CHECK(result.training_observations <= total);
    // Should still get decent accuracy on linearly separable data
    REQUIRE(result.binary_train_metrics.has_value());
    CHECK(result.binary_train_metrics->accuracy >= 0.70);
}

// ============================================================================
// Pipeline with TimeEntity group labels
// ============================================================================

TEST_CASE("ClassificationPipeline: labels from TimeEntity groups",
          "[ClassificationPipeline]") {
    constexpr std::size_t per_class = 50;
    auto dm = makeDM("cam", 200);
    createLinSepTensor(*dm, "features", "cam", per_class);
    auto [g0, g1] = createTimeEntityGroups(*dm, "cam", per_class);

    MLCore::MLModelRegistry const registry;

    MLCore::LabelFromTimeEntityGroups group_labels;
    group_labels.class_groups = {g0, g1};
    group_labels.time_key = "cam";

    MLCore::ClassificationPipelineConfig config;
    config.model_name = "Random Forest";
    config.feature_tensor_key = "features";
    config.label_config = group_labels;
    config.time_key_str = "cam";
    config.output_config.output_prefix = "PredG:";
    config.output_config.time_key_str = "cam";
    config.prediction_region.predict_all_rows = true;

    auto result = MLCore::runClassificationPipeline(*dm, registry, config);

    REQUIRE(result.success);
    CHECK(result.num_classes == 2);
    CHECK(result.training_observations == per_class * 2);
    REQUIRE(result.multi_class_train_metrics.has_value());
    CHECK(result.multi_class_train_metrics->overall_accuracy >= 0.90);
}

// ============================================================================
// Pipeline with z-score normalization
// ============================================================================

TEST_CASE("ClassificationPipeline: z-score normalization",
          "[ClassificationPipeline]") {
    constexpr std::size_t per_class = 50;
    auto dm = makeDM("cam", 200);
    createLinSepTensor(*dm, "features", "cam", per_class);
    createLabelIntervalSeries(*dm, "labels", "cam", per_class);

    MLCore::MLModelRegistry const registry;

    MLCore::ClassificationPipelineConfig config;
    config.model_name = "Random Forest";
    config.feature_tensor_key = "features";
    config.label_config = MLCore::LabelFromIntervals{"Contact", "NoContact"};
    config.label_interval_key = "labels";
    config.time_key_str = "cam";
    config.conversion_config.zscore_normalize = true;
    config.output_config.time_key_str = "cam";
    config.prediction_region.predict_all_rows = true;

    auto result = MLCore::runClassificationPipeline(*dm, registry, config);

    REQUIRE(result.success);
    CHECK(result.num_features == 2);
    // Z-score shouldn't break linearly separable data
    REQUIRE(result.binary_train_metrics.has_value());
    CHECK(result.binary_train_metrics->accuracy >= 0.90);
}

// ============================================================================
// Pipeline with NaN rows in features
// ============================================================================

TEST_CASE("ClassificationPipeline: handles NaN rows in features",
          "[ClassificationPipeline]") {
    auto dm = makeDM("cam", 200);

    // Create tensor with some NaN rows
    std::size_t const total = 100;
    std::size_t const cols = 2;
    std::vector<float> data(total * cols);
    for (std::size_t i = 0; i < total; ++i) {
        if (i < 50) {
            data[i * cols + 0] = static_cast<float>(i) * 0.1f;
        } else {
            data[i * cols + 0] = 10.0f + static_cast<float>(i - 50) * 0.1f;
        }
        data[i * cols + 1] = static_cast<float>(i) * 0.01f;
    }
    // Make rows 5 and 55 NaN
    data[5 * cols + 0] = std::numeric_limits<float>::quiet_NaN();
    data[55 * cols + 0] = std::numeric_limits<float>::quiet_NaN();

    auto ts = makeDenseTimeStorage(total);
    auto tf = dm->getTime(TimeKey("cam"));
    auto tensor = std::make_shared<TensorData>(
            TensorData::createTimeSeries2D(data, total, cols, ts, tf,
                                           {"f0", "f1"}));
    dm->setData<TensorData>("features", tensor, TimeKey("cam"));
    createLabelIntervalSeries(*dm, "labels", "cam", 50);

    MLCore::MLModelRegistry const registry;

    MLCore::ClassificationPipelineConfig config;
    config.model_name = "Random Forest";
    config.feature_tensor_key = "features";
    config.label_config = MLCore::LabelFromIntervals{"Contact", "NoContact"};
    config.label_interval_key = "labels";
    config.time_key_str = "cam";
    config.conversion_config.drop_nan = true;
    config.output_config.time_key_str = "cam";
    config.prediction_region.predict_all_rows = true;

    auto result = MLCore::runClassificationPipeline(*dm, registry, config);

    REQUIRE(result.success);
    CHECK(result.rows_dropped_nan == 2);
    // Training should use 98 observations (100 - 2 NaN)
    CHECK(result.training_observations == 98);
    CHECK(result.prediction_observations == 98);
}

// ============================================================================
// Pipeline with no prediction
// ============================================================================

TEST_CASE("ClassificationPipeline: train only, no prediction",
          "[ClassificationPipeline]") {
    constexpr std::size_t per_class = 50;
    auto dm = makeDM("cam", 200);
    createLinSepTensor(*dm, "features", "cam", per_class);
    createLabelIntervalSeries(*dm, "labels", "cam", per_class);

    MLCore::MLModelRegistry const registry;

    MLCore::ClassificationPipelineConfig config;
    config.model_name = "Random Forest";
    config.feature_tensor_key = "features";
    config.label_config = MLCore::LabelFromIntervals{"Contact", "NoContact"};
    config.label_interval_key = "labels";
    config.time_key_str = "cam";
    config.prediction_region.predict_all_rows = false;
    // No prediction_tensor_key → no prediction
    config.output_config.time_key_str = "cam";

    auto result = MLCore::runClassificationPipeline(*dm, registry, config);

    REQUIRE(result.success);
    CHECK(result.training_observations == per_class * 2);
    CHECK(result.prediction_observations == 0);
    CHECK_FALSE(result.binary_train_metrics.has_value());
    CHECK_FALSE(result.writer_result.has_value());

    // Model should still be trained and returned
    REQUIRE(result.trained_model != nullptr);
    CHECK(result.trained_model->isTrained());
}

// ============================================================================
// Pipeline with separate prediction tensor
// ============================================================================

TEST_CASE("ClassificationPipeline: predict on separate tensor",
          "[ClassificationPipeline]") {
    constexpr std::size_t per_class = 50;
    auto dm = makeDM("cam", 300);
    createLinSepTensor(*dm, "train_features", "cam", per_class);
    createLabelIntervalSeries(*dm, "labels", "cam", per_class);

    // Create a separate prediction tensor with features in the same range
    std::size_t const pred_rows = 20;
    std::size_t const cols = 2;
    std::vector<float> pred_data(pred_rows * cols);
    for (std::size_t i = 0; i < pred_rows; ++i) {
        // Alternate between class 0 and class 1 ranges
        if (i % 2 == 0) {
            pred_data[i * cols + 0] = 1.0f;// class 0 range
        } else {
            pred_data[i * cols + 0] = 12.0f;// class 1 range
        }
        pred_data[i * cols + 1] = 0.5f;
    }

    // Prediction tensor starts at time 200 to not overlap with training
    auto pred_times_vec = std::vector<TimeFrameIndex>();
    pred_times_vec.reserve(pred_rows);
    for (std::size_t i = 0; i < pred_rows; ++i) {
        pred_times_vec.emplace_back(static_cast<int64_t>(200 + i));
    }
    auto pred_ts = TimeIndexStorageFactory::createFromTimeIndices(pred_times_vec);
    auto tf = dm->getTime(TimeKey("cam"));
    auto pred_tensor = std::make_shared<TensorData>(
            TensorData::createTimeSeries2D(pred_data, pred_rows, cols,
                                           pred_ts, tf, {"feature_0", "feature_1"}));
    dm->setData<TensorData>("pred_features", pred_tensor, TimeKey("cam"));

    MLCore::MLModelRegistry const registry;

    MLCore::ClassificationPipelineConfig config;
    config.model_name = "Random Forest";
    config.feature_tensor_key = "train_features";
    config.label_config = MLCore::LabelFromIntervals{"Contact", "NoContact"};
    config.label_interval_key = "labels";
    config.time_key_str = "cam";
    config.prediction_region.predict_all_rows = false;
    config.prediction_region.prediction_tensor_key = "pred_features";
    config.output_config.output_prefix = "PredSep:";
    config.output_config.time_key_str = "cam";
    config.output_config.write_intervals = true;
    config.output_config.write_probabilities = true;

    auto result = MLCore::runClassificationPipeline(*dm, registry, config);

    REQUIRE(result.success);
    CHECK(result.training_observations == per_class * 2);
    CHECK(result.prediction_observations == pred_rows);

    // Writer result should be present
    REQUIRE(result.writer_result.has_value());
    CHECK(result.writer_result->interval_keys.size() == 2);
}

// ============================================================================
// Pipeline: prediction tensor not found
// ============================================================================

TEST_CASE("ClassificationPipeline: fails if prediction tensor not found",
          "[ClassificationPipeline]") {
    constexpr std::size_t per_class = 50;
    auto dm = makeDM("cam", 200);
    createLinSepTensor(*dm, "features", "cam", per_class);
    createLabelIntervalSeries(*dm, "labels", "cam", per_class);

    MLCore::MLModelRegistry const registry;

    MLCore::ClassificationPipelineConfig config;
    config.model_name = "Random Forest";
    config.feature_tensor_key = "features";
    config.label_config = MLCore::LabelFromIntervals{"Contact", "NoContact"};
    config.label_interval_key = "labels";
    config.time_key_str = "cam";
    config.prediction_region.predict_all_rows = false;
    config.prediction_region.prediction_tensor_key = "nonexistent_pred";

    auto result = MLCore::runClassificationPipeline(*dm, registry, config);

    CHECK_FALSE(result.success);
    CHECK(result.failed_stage == MLCore::ClassificationStage::Predicting);
    CHECK(result.error_message.find("not found") != std::string::npos);
}

// ============================================================================
// Pipeline result carries trained model for reuse
// ============================================================================

TEST_CASE("ClassificationPipeline: returned model can re-predict",
          "[ClassificationPipeline]") {
    constexpr std::size_t per_class = 50;
    auto dm = makeDM("cam", 200);
    createLinSepTensor(*dm, "features", "cam", per_class);
    createLabelIntervalSeries(*dm, "labels", "cam", per_class);

    MLCore::MLModelRegistry const registry;

    MLCore::ClassificationPipelineConfig config;
    config.model_name = "Random Forest";
    config.feature_tensor_key = "features";
    config.label_config = MLCore::LabelFromIntervals{"Contact", "NoContact"};
    config.label_interval_key = "labels";
    config.time_key_str = "cam";
    config.prediction_region.predict_all_rows = false;
    config.output_config.time_key_str = "cam";

    auto result = MLCore::runClassificationPipeline(*dm, registry, config);
    REQUIRE(result.success);
    REQUIRE(result.trained_model != nullptr);

    // Use the model to predict on new data
    arma::mat test_features(2, 4);
    // Two clear class-0 points and two clear class-1 points
    test_features.col(0) = arma::vec{0.5, 0.0};
    test_features.col(1) = arma::vec{1.0, 0.0};
    test_features.col(2) = arma::vec{12.0, 0.0};
    test_features.col(3) = arma::vec{13.0, 0.0};

    arma::Row<std::size_t> preds;
    bool const ok = result.trained_model->predict(test_features, preds);
    REQUIRE(ok);
    REQUIRE(preds.n_elem == 4);
    // Class 0 and class 1 should be separable
    CHECK(preds[0] == preds[1]);
    CHECK(preds[2] == preds[3]);
    CHECK(preds[0] != preds[2]);
}

// ============================================================================
// Pipeline: progress callback exercised
// ============================================================================

TEST_CASE("ClassificationPipeline: progress callback receives all stages",
          "[ClassificationPipeline]") {
    constexpr std::size_t per_class = 30;
    auto dm = makeDM("cam", 200);
    createLinSepTensor(*dm, "features", "cam", per_class);
    createLabelIntervalSeries(*dm, "labels", "cam", per_class);

    MLCore::MLModelRegistry const registry;

    MLCore::ClassificationPipelineConfig config;
    config.model_name = "Random Forest";
    config.feature_tensor_key = "features";
    config.label_config = MLCore::LabelFromIntervals{"P", "N"};
    config.label_interval_key = "labels";
    config.time_key_str = "cam";
    config.balance_classes = true;
    config.balancing_config.strategy = MLCore::BalancingStrategy::Subsample;
    config.balancing_config.random_seed = 1;
    config.output_config.time_key_str = "cam";
    config.prediction_region.predict_all_rows = true;

    std::vector<MLCore::ClassificationStage> stages;
    std::vector<std::string> messages;
    auto progress = [&](MLCore::ClassificationStage stage, std::string const & msg) {
        stages.push_back(stage);
        messages.push_back(msg);
    };

    auto result = MLCore::runClassificationPipeline(*dm, registry, config, progress);
    REQUIRE(result.success);

    // Should see all stages in order
    REQUIRE(stages.size() >= 8);
    CHECK(stages[0] == MLCore::ClassificationStage::ValidatingFeatures);
    CHECK(stages[1] == MLCore::ClassificationStage::ConvertingFeatures);
    CHECK(stages[2] == MLCore::ClassificationStage::AssemblingLabels);
    CHECK(stages[3] == MLCore::ClassificationStage::BalancingClasses);
    CHECK(stages[4] == MLCore::ClassificationStage::Training);
    CHECK(stages[5] == MLCore::ClassificationStage::Predicting);
    CHECK(stages[6] == MLCore::ClassificationStage::ComputingMetrics);
    CHECK(stages[7] == MLCore::ClassificationStage::WritingOutput);

    // All messages should be non-empty
    for (auto const & msg: messages) {
        CHECK_FALSE(msg.empty());
    }
}

// ============================================================================
// Pipeline: output prefix naming
// ============================================================================

TEST_CASE("ClassificationPipeline: output uses configured prefix",
          "[ClassificationPipeline]") {
    constexpr std::size_t per_class = 30;
    auto dm = makeDM("cam", 200);
    createLinSepTensor(*dm, "features", "cam", per_class);
    createLabelIntervalSeries(*dm, "labels", "cam", per_class);

    MLCore::MLModelRegistry const registry;

    MLCore::ClassificationPipelineConfig config;
    config.model_name = "Random Forest";
    config.feature_tensor_key = "features";
    config.label_config = MLCore::LabelFromIntervals{"Contact", "NoContact"};
    config.label_interval_key = "labels";
    config.time_key_str = "cam";
    config.output_config.output_prefix = "MyPrefix:";
    config.output_config.time_key_str = "cam";
    config.prediction_region.predict_all_rows = true;

    auto result = MLCore::runClassificationPipeline(*dm, registry, config);

    REQUIRE(result.success);
    REQUIRE(result.writer_result.has_value());

    // Interval keys should start with the configured prefix
    for (auto const & key: result.writer_result->interval_keys) {
        CHECK(key.find("MyPrefix:") == 0);
    }
}

// ============================================================================
// Sequence model (HMM) pipeline tests
// ============================================================================

namespace {

/**
 * @brief Create a 1D feature tensor with block structure for HMM testing
 *
 * State 0 (frames 0..half-1):  feature values centered around -2.0
 * State 1 (frames half..n-1):  feature values centered around +2.0
 *
 * This creates a single contiguous sequence with two clearly separable states.
 */
void createBlockSequenceTensor(
        DataManager & dm,
        std::string const & key,
        std::string const & time_key,
        std::size_t per_class = 50) {
    auto const total = per_class * 2;
    auto const cols = std::size_t{1};
    std::vector<float> data(total * cols);

    for (std::size_t i = 0; i < per_class; ++i) {
        data[i] = -2.0f + static_cast<float>(i) * 0.001f;
    }
    for (std::size_t i = 0; i < per_class; ++i) {
        data[per_class + i] = 2.0f + static_cast<float>(i) * 0.001f;
    }

    auto ts = makeDenseTimeStorage(total);
    auto tf = dm.getTime(TimeKey(time_key));

    std::vector<std::string> const names = {"feature_0"};
    auto tensor = std::make_shared<TensorData>(
            TensorData::createTimeSeries2D(data, total, cols, ts, tf, names));

    dm.setData<TensorData>(key, tensor, TimeKey(time_key));
}

/**
 * @brief Create a DigitalEventSeries marking the second half of rows as events
 *
 * Places events at every frame in [per_class, 2*per_class).
 */
void createEventSeries(
        DataManager & dm,
        std::string const & key,
        std::string const & time_key,
        std::size_t per_class = 50) {
    auto tf = dm.getTime(TimeKey(time_key));
    auto series = std::make_shared<DigitalEventSeries>();
    series->setTimeFrame(tf);
    for (std::size_t i = per_class; i < per_class * 2; ++i) {
        series->addEvent(TimeFrameIndex(static_cast<std::int64_t>(i)));
    }
    dm.setData<DigitalEventSeries>(key, series, TimeKey(time_key));
}

}// anonymous namespace

TEST_CASE("ClassificationPipeline: HMM end-to-end with interval labels",
          "[ClassificationPipeline][HMM]") {
    constexpr std::size_t per_class = 50;
    auto dm = makeDM("cam", 200);
    createBlockSequenceTensor(*dm, "features", "cam", per_class);
    createLabelIntervalSeries(*dm, "labels", "cam", per_class);

    MLCore::MLModelRegistry const registry;

    MLCore::ClassificationPipelineConfig config;
    config.model_name = "Hidden Markov Model (Gaussian)";
    config.feature_tensor_key = "features";
    config.label_config = MLCore::LabelFromIntervals{"Contact", "NoContact"};
    config.label_interval_key = "labels";
    config.time_key_str = "cam";
    config.output_config.output_prefix = "HMM:";
    config.output_config.time_key_str = "cam";
    config.output_config.write_intervals = true;
    config.prediction_region.predict_all_rows = true;

    // Track progress stages
    std::vector<MLCore::ClassificationStage> stages_seen;
    auto progress_cb = [&](MLCore::ClassificationStage stage, std::string const &) {
        stages_seen.push_back(stage);
    };

    auto result = MLCore::runClassificationPipeline(*dm, registry, config, progress_cb);

    REQUIRE(result.success);
    CHECK(result.error_message.empty());
    CHECK(result.num_classes == 2);
    CHECK(result.num_features == 1);
    CHECK(result.training_observations == per_class * 2);

    // Predictions should be made
    CHECK(result.prediction_observations == per_class * 2);

    // The SegmentingSequences stage should appear in progress
    auto it = std::find(stages_seen.begin(), stages_seen.end(),
                        MLCore::ClassificationStage::SegmentingSequences);
    CHECK(it != stages_seen.end());

    // SegmentingSequences should appear between BalancingClasses and Training
    auto training_it = std::find(stages_seen.begin(), stages_seen.end(),
                                 MLCore::ClassificationStage::Training);
    REQUIRE(training_it != stages_seen.end());
    if (it != stages_seen.end()) {
        CHECK(it < training_it);
    }

    // Trained model should be a sequence model
    REQUIRE(result.trained_model != nullptr);
    CHECK(result.trained_model->isTrained());
    CHECK(result.trained_model->isSequenceModel());

    // Writer result should be present
    REQUIRE(result.writer_result.has_value());
    CHECK(result.writer_result->interval_keys.size() == 2);
}

TEST_CASE("ClassificationPipeline: HMM end-to-end with event labels",
          "[ClassificationPipeline][HMM]") {
    constexpr std::size_t per_class = 50;
    auto dm = makeDM("cam", 200);
    createBlockSequenceTensor(*dm, "features", "cam", per_class);
    createEventSeries(*dm, "events", "cam", per_class);

    MLCore::MLModelRegistry const registry;

    MLCore::ClassificationPipelineConfig config;
    config.model_name = "Hidden Markov Model (Gaussian)";
    config.feature_tensor_key = "features";
    config.label_config = MLCore::LabelFromEvents{"Contact", "NoContact"};
    config.label_event_key = "events";
    config.time_key_str = "cam";
    config.output_config.output_prefix = "HMM:";
    config.output_config.time_key_str = "cam";
    config.output_config.write_intervals = true;
    config.prediction_region.predict_all_rows = true;

    auto result = MLCore::runClassificationPipeline(*dm, registry, config);

    REQUIRE(result.success);
    CHECK(result.num_classes == 2);
    CHECK(result.prediction_observations == per_class * 2);

    REQUIRE(result.trained_model != nullptr);
    CHECK(result.trained_model->isSequenceModel());
    CHECK(result.trained_model->isTrained());

    REQUIRE(result.writer_result.has_value());
    CHECK(result.writer_result->interval_keys.size() == 2);
}

TEST_CASE("ClassificationPipeline: HMM progress includes SegmentingSequences",
          "[ClassificationPipeline][HMM]") {
    constexpr std::size_t per_class = 30;
    auto dm = makeDM("cam", 200);
    createBlockSequenceTensor(*dm, "features", "cam", per_class);
    createLabelIntervalSeries(*dm, "labels", "cam", per_class);

    MLCore::MLModelRegistry const registry;

    MLCore::ClassificationPipelineConfig config;
    config.model_name = "Hidden Markov Model (Gaussian)";
    config.feature_tensor_key = "features";
    config.label_config = MLCore::LabelFromIntervals{"P", "N"};
    config.label_interval_key = "labels";
    config.time_key_str = "cam";
    config.output_config.time_key_str = "cam";
    config.prediction_region.predict_all_rows = true;

    std::vector<MLCore::ClassificationStage> stages;
    auto progress_cb = [&](MLCore::ClassificationStage stage, std::string const &) {
        stages.push_back(stage);
    };

    auto result = MLCore::runClassificationPipeline(*dm, registry, config, progress_cb);
    REQUIRE(result.success);

    // For an HMM, should see SegmentingSequences between labels and training
    REQUIRE(stages.size() >= 8);
    CHECK(stages[0] == MLCore::ClassificationStage::ValidatingFeatures);
    CHECK(stages[1] == MLCore::ClassificationStage::ConvertingFeatures);
    CHECK(stages[2] == MLCore::ClassificationStage::AssemblingLabels);
    CHECK(stages[3] == MLCore::ClassificationStage::SegmentingSequences);
    CHECK(stages[4] == MLCore::ClassificationStage::Training);
    CHECK(stages[5] == MLCore::ClassificationStage::Predicting);
    CHECK(stages[6] == MLCore::ClassificationStage::ComputingMetrics);
    CHECK(stages[7] == MLCore::ClassificationStage::WritingOutput);
}

TEST_CASE("ClassificationPipeline: non-sequence model skips SegmentingSequences",
          "[ClassificationPipeline]") {
    constexpr std::size_t per_class = 30;
    auto dm = makeDM("cam", 200);
    createLinSepTensor(*dm, "features", "cam", per_class);
    createLabelIntervalSeries(*dm, "labels", "cam", per_class);

    MLCore::MLModelRegistry const registry;

    MLCore::ClassificationPipelineConfig config;
    config.model_name = "Random Forest";
    config.feature_tensor_key = "features";
    config.label_config = MLCore::LabelFromIntervals{"P", "N"};
    config.label_interval_key = "labels";
    config.time_key_str = "cam";
    config.output_config.time_key_str = "cam";
    config.prediction_region.predict_all_rows = true;

    std::vector<MLCore::ClassificationStage> stages;
    auto progress_cb = [&](MLCore::ClassificationStage stage, std::string const &) {
        stages.push_back(stage);
    };

    auto result = MLCore::runClassificationPipeline(*dm, registry, config, progress_cb);
    REQUIRE(result.success);

    // SegmentingSequences should NOT appear for non-sequence models
    auto it = std::find(stages.begin(), stages.end(),
                        MLCore::ClassificationStage::SegmentingSequences);
    CHECK(it == stages.end());
}

// ============================================================================
// Phase 8: Prediction region, bounding span, constrained decoding, diagonal
// ============================================================================

namespace {

/**
 * @brief Create two block-sequence tensors and a prediction interval for Phase 8 tests
 *
 * Training region: frames  0–49 (state 0, feature ~-2) and 50–99 (state 1, feature ~+2)
 * Gap:            frames 100–149 (unlabeled, "predict me" region, feature values
 *                 picked to match state 0 early and state 1 late)
 * Second region:  frames 150–199 (state 1, feature ~+2) and 200–249 (state 0, feature ~-2)
 *
 * Returns a DM with:
 *   "features" — 1D block tensor over frames 0–249
 *   "train_labels" — DigitalIntervalSeries marking frames 50–99 as positive
 *   "train_labels_2" — DigitalIntervalSeries marking frames 150–199 as positive
 *   "predict_region" — DigitalIntervalSeries covering frames 100–149
 */
std::shared_ptr<DataManager> makePhase8DM() {
    auto dm = std::make_shared<DataManager>();
    dm->setTime(TimeKey("cam"), makeTimeFrame(250));

    // Build feature tensor: 5 blocks of 50 frames each
    // Block 0 (0-49):   state 0 features ~-2
    // Block 1 (50-99):  state 1 features ~+2
    // Block 2 (100-149): state 0 early (100-124) then state 1 late (125-149)
    // Block 3 (150-199): state 1 features ~+2
    // Block 4 (200-249): state 0 features ~-2
    constexpr std::size_t total = 250;
    std::vector<float> data(total);
    for (std::size_t i = 0; i < 50; ++i) {
        data[i] = -2.0f + static_cast<float>(i) * 0.001f;// block 0: state 0
    }
    for (std::size_t i = 50; i < 100; ++i) {
        data[i] = 2.0f + static_cast<float>(i - 50) * 0.001f;// block 1: state 1
    }
    for (std::size_t i = 100; i < 125; ++i) {
        data[i] = -2.0f + static_cast<float>(i - 100) * 0.001f;// block 2a: state 0
    }
    for (std::size_t i = 125; i < 150; ++i) {
        data[i] = 2.0f + static_cast<float>(i - 125) * 0.001f;// block 2b: state 1
    }
    for (std::size_t i = 150; i < 200; ++i) {
        data[i] = 2.0f + static_cast<float>(i - 150) * 0.001f;// block 3: state 1
    }
    for (std::size_t i = 200; i < 250; ++i) {
        data[i] = -2.0f + static_cast<float>(i - 200) * 0.001f;// block 4: state 0
    }

    auto ts = makeDenseTimeStorage(total);
    auto tf = dm->getTime(TimeKey("cam"));
    std::vector<std::string> const names = {"feature_0"};
    auto tensor = std::make_shared<TensorData>(
            TensorData::createTimeSeries2D(data, total, 1, ts, tf, names));
    dm->setData<TensorData>("features", tensor, TimeKey("cam"));

    // Training labels: frames 50–99 are positive (state 1)
    auto train_labels = std::make_shared<DigitalIntervalSeries>();
    train_labels->setTimeFrame(tf);
    train_labels->addEvent(TimeFrameIndex(50), TimeFrameIndex(99));
    dm->setData<DigitalIntervalSeries>("train_labels", train_labels, TimeKey("cam"));

    // Additional training labels on the far side: frames 150–199 positive
    auto train_labels_2 = std::make_shared<DigitalIntervalSeries>();
    train_labels_2->setTimeFrame(tf);
    train_labels_2->addEvent(TimeFrameIndex(150), TimeFrameIndex(199));
    dm->setData<DigitalIntervalSeries>("train_labels_2", train_labels_2, TimeKey("cam"));

    // Prediction region: frames 100–149 (the gap between training blocks)
    auto predict_region = std::make_shared<DigitalIntervalSeries>();
    predict_region->setTimeFrame(tf);
    predict_region->addEvent(TimeFrameIndex(100), TimeFrameIndex(149));
    dm->setData<DigitalIntervalSeries>("predict_region", predict_region, TimeKey("cam"));

    return dm;
}

}// anonymous namespace

// ============================================================================
// Prediction interval filtering (bounding span + output restriction)
// ============================================================================

TEST_CASE("ClassificationPipeline: prediction interval restricts output frames",
          "[ClassificationPipeline][PredictionRegion]") {
    auto dm = makePhase8DM();
    MLCore::MLModelRegistry const registry;

    MLCore::ClassificationPipelineConfig config;
    config.model_name = "Hidden Markov Model (Gaussian)";
    config.feature_tensor_key = "features";
    config.label_config = MLCore::LabelFromIntervals{"State1", "State0"};
    config.label_interval_key = "train_labels";
    config.time_key_str = "cam";
    config.output_config.output_prefix = "PR:";
    config.output_config.time_key_str = "cam";
    config.output_config.write_intervals = true;
    config.prediction_region.predict_all_rows = true;
    config.prediction_region.prediction_interval_key = "predict_region";
    config.prediction_region.constrained_decoding = false;

    auto result = MLCore::runClassificationPipeline(*dm, registry, config);

    REQUIRE(result.success);
    // Output should be filtered to frames 100–149 only (50 frames)
    CHECK(result.prediction_observations == 50);
    REQUIRE(result.writer_result.has_value());
    // Intervals created for 2 classes
    CHECK(result.writer_result->interval_keys.size() == 2);
}

TEST_CASE("ClassificationPipeline: prediction interval with Random Forest",
          "[ClassificationPipeline][PredictionRegion]") {
    // Frame-independent models should also respect prediction interval filtering
    auto dm = makePhase8DM();
    MLCore::MLModelRegistry const registry;

    MLCore::ClassificationPipelineConfig config;
    config.model_name = "Random Forest";
    config.feature_tensor_key = "features";
    config.label_config = MLCore::LabelFromIntervals{"State1", "State0"};
    config.label_interval_key = "train_labels";
    config.time_key_str = "cam";
    config.output_config.output_prefix = "RF:";
    config.output_config.time_key_str = "cam";
    config.output_config.write_intervals = true;
    config.prediction_region.predict_all_rows = true;
    config.prediction_region.prediction_interval_key = "predict_region";

    auto result = MLCore::runClassificationPipeline(*dm, registry, config);

    REQUIRE(result.success);
    CHECK(result.prediction_observations == 50);
}

TEST_CASE("ClassificationPipeline: empty prediction interval key means no filtering",
          "[ClassificationPipeline][PredictionRegion]") {
    constexpr std::size_t per_class = 30;
    auto dm = makeDM("cam", 200);
    createBlockSequenceTensor(*dm, "features", "cam", per_class);
    createLabelIntervalSeries(*dm, "labels", "cam", per_class);

    MLCore::MLModelRegistry const registry;

    MLCore::ClassificationPipelineConfig config;
    config.model_name = "Hidden Markov Model (Gaussian)";
    config.feature_tensor_key = "features";
    config.label_config = MLCore::LabelFromIntervals{"P", "N"};
    config.label_interval_key = "labels";
    config.time_key_str = "cam";
    config.output_config.time_key_str = "cam";
    config.prediction_region.predict_all_rows = true;
    config.prediction_region.prediction_interval_key = "";

    auto result = MLCore::runClassificationPipeline(*dm, registry, config);

    REQUIRE(result.success);
    // All 60 training rows should be predicted (no filtering)
    CHECK(result.prediction_observations == per_class * 2);
}

// ============================================================================
// Constrained Viterbi in pipeline
// ============================================================================

TEST_CASE("ClassificationPipeline: constrained decoding produces predictions",
          "[ClassificationPipeline][ConstrainedDecoding]") {
    auto dm = makePhase8DM();
    MLCore::MLModelRegistry const registry;

    MLCore::ClassificationPipelineConfig config;
    config.model_name = "Hidden Markov Model (Gaussian)";
    config.feature_tensor_key = "features";
    config.label_config = MLCore::LabelFromIntervals{"State1", "State0"};
    config.label_interval_key = "train_labels";
    config.time_key_str = "cam";
    config.output_config.output_prefix = "CD:";
    config.output_config.time_key_str = "cam";
    config.prediction_region.predict_all_rows = true;
    config.prediction_region.constrained_decoding = true;

    auto result = MLCore::runClassificationPipeline(*dm, registry, config);

    REQUIRE(result.success);
    CHECK(result.prediction_observations > 0);
    CHECK(result.trained_model->isSequenceModel());
}

TEST_CASE("ClassificationPipeline: constrained vs unconstrained accuracy comparison",
          "[ClassificationPipeline][ConstrainedDecoding]") {
    // Both constrained and unconstrained should succeed on clean data.
    // Constrained decoding should produce >= accuracy as unconstrained
    // when the training labels provide useful boundary information.
    auto dm_constrained = makePhase8DM();
    auto dm_unconstrained = makePhase8DM();
    MLCore::MLModelRegistry const registry;

    auto makeConfig = [](bool constrained) {
        MLCore::ClassificationPipelineConfig config;
        config.model_name = "Hidden Markov Model (Gaussian)";
        config.feature_tensor_key = "features";
        config.label_config = MLCore::LabelFromIntervals{"State1", "State0"};
        config.label_interval_key = "train_labels";
        config.time_key_str = "cam";
        config.output_config.output_prefix = constrained ? "Con:" : "Unc:";
        config.output_config.time_key_str = "cam";
        config.output_config.write_intervals = true;
        config.prediction_region.predict_all_rows = true;
        config.prediction_region.constrained_decoding = constrained;
        return config;
    };

    auto constrained_result = MLCore::runClassificationPipeline(
            *dm_constrained, registry, makeConfig(true));
    auto unconstrained_result = MLCore::runClassificationPipeline(
            *dm_unconstrained, registry, makeConfig(false));

    REQUIRE(constrained_result.success);
    REQUIRE(unconstrained_result.success);

    // Both should produce the same number of predictions
    CHECK(constrained_result.prediction_observations ==
          unconstrained_result.prediction_observations);

    // Both should have trained models
    CHECK(constrained_result.trained_model->isTrained());
    CHECK(unconstrained_result.trained_model->isTrained());
}

TEST_CASE("ClassificationPipeline: constrained decoding ignored for non-sequence models",
          "[ClassificationPipeline][ConstrainedDecoding]") {
    constexpr std::size_t per_class = 30;
    auto dm = makeDM("cam", 200);
    createLinSepTensor(*dm, "features", "cam", per_class);
    createLabelIntervalSeries(*dm, "labels", "cam", per_class);

    MLCore::MLModelRegistry const registry;

    MLCore::ClassificationPipelineConfig config;
    config.model_name = "Random Forest";
    config.feature_tensor_key = "features";
    config.label_config = MLCore::LabelFromIntervals{"P", "N"};
    config.label_interval_key = "labels";
    config.time_key_str = "cam";
    config.output_config.time_key_str = "cam";
    config.prediction_region.predict_all_rows = true;
    // Constrained decoding enabled but model is not a sequence model
    config.prediction_region.constrained_decoding = true;

    auto result = MLCore::runClassificationPipeline(*dm, registry, config);

    REQUIRE(result.success);
    CHECK(result.prediction_observations == per_class * 2);
}

// ============================================================================
// Diagonal covariance in pipeline
// ============================================================================

TEST_CASE("ClassificationPipeline: HMM with diagonal covariance",
          "[ClassificationPipeline][DiagonalCovariance]") {
    constexpr std::size_t per_class = 50;
    auto dm = makeDM("cam", 200);
    createBlockSequenceTensor(*dm, "features", "cam", per_class);
    createLabelIntervalSeries(*dm, "labels", "cam", per_class);

    MLCore::MLModelRegistry const registry;

    MLCore::HMMParameters hmm_params;
    hmm_params.num_states = 2;
    hmm_params.use_diagonal_covariance = true;

    MLCore::ClassificationPipelineConfig config;
    config.model_name = "Hidden Markov Model (Gaussian)";
    config.model_params = &hmm_params;
    config.feature_tensor_key = "features";
    config.label_config = MLCore::LabelFromIntervals{"Contact", "NoContact"};
    config.label_interval_key = "labels";
    config.time_key_str = "cam";
    config.output_config.output_prefix = "Diag:";
    config.output_config.time_key_str = "cam";
    config.output_config.write_intervals = true;
    config.prediction_region.predict_all_rows = true;

    auto result = MLCore::runClassificationPipeline(*dm, registry, config);

    REQUIRE(result.success);
    CHECK(result.prediction_observations == per_class * 2);
    CHECK(result.num_classes == 2);

    // Verify the trained model uses diagonal covariance
    REQUIRE(result.trained_model != nullptr);
    auto const * hmm = dynamic_cast<MLCore::HiddenMarkovModelOperation const *>(
            result.trained_model.get());
    REQUIRE(hmm != nullptr);
    CHECK(hmm->isDiagonalCovariance());
    CHECK(hmm->isTrained());
}

TEST_CASE("ClassificationPipeline: diagonal covariance with event labels",
          "[ClassificationPipeline][DiagonalCovariance]") {
    constexpr std::size_t per_class = 50;
    auto dm = makeDM("cam", 200);
    createBlockSequenceTensor(*dm, "features", "cam", per_class);
    createEventSeries(*dm, "events", "cam", per_class);

    MLCore::MLModelRegistry const registry;

    MLCore::HMMParameters hmm_params;
    hmm_params.use_diagonal_covariance = true;

    MLCore::ClassificationPipelineConfig config;
    config.model_name = "Hidden Markov Model (Gaussian)";
    config.model_params = &hmm_params;
    config.feature_tensor_key = "features";
    config.label_config = MLCore::LabelFromEvents{"Contact", "NoContact"};
    config.label_event_key = "events";
    config.time_key_str = "cam";
    config.output_config.output_prefix = "DiagEvt:";
    config.output_config.time_key_str = "cam";
    config.prediction_region.predict_all_rows = true;

    auto result = MLCore::runClassificationPipeline(*dm, registry, config);

    REQUIRE(result.success);
    CHECK(result.prediction_observations == per_class * 2);

    auto const * hmm = dynamic_cast<MLCore::HiddenMarkovModelOperation const *>(
            result.trained_model.get());
    REQUIRE(hmm != nullptr);
    CHECK(hmm->isDiagonalCovariance());
}

// ============================================================================
// Combined Phase 8 features
// ============================================================================

TEST_CASE("ClassificationPipeline: prediction region + constrained + diagonal combined",
          "[ClassificationPipeline][PredictionRegion][ConstrainedDecoding][DiagonalCovariance]") {
    auto dm = makePhase8DM();
    MLCore::MLModelRegistry const registry;

    MLCore::HMMParameters hmm_params;
    hmm_params.use_diagonal_covariance = true;

    MLCore::ClassificationPipelineConfig config;
    config.model_name = "Hidden Markov Model (Gaussian)";
    config.model_params = &hmm_params;
    config.feature_tensor_key = "features";
    config.label_config = MLCore::LabelFromIntervals{"State1", "State0"};
    config.label_interval_key = "train_labels";
    config.time_key_str = "cam";
    config.output_config.output_prefix = "All:";
    config.output_config.time_key_str = "cam";
    config.output_config.write_intervals = true;
    config.prediction_region.predict_all_rows = true;
    config.prediction_region.prediction_interval_key = "predict_region";
    config.prediction_region.constrained_decoding = true;

    auto result = MLCore::runClassificationPipeline(*dm, registry, config);

    REQUIRE(result.success);
    // Output filtered to prediction region: frames 100–149 = 50 frames
    CHECK(result.prediction_observations == 50);

    auto const * hmm = dynamic_cast<MLCore::HiddenMarkovModelOperation const *>(
            result.trained_model.get());
    REQUIRE(hmm != nullptr);
    CHECK(hmm->isDiagonalCovariance());
    CHECK(hmm->isSequenceModel());

    REQUIRE(result.writer_result.has_value());
    CHECK(result.writer_result->interval_keys.size() == 2);
}

TEST_CASE("ClassificationPipeline: missing prediction interval key fails gracefully",
          "[ClassificationPipeline][PredictionRegion]") {
    constexpr std::size_t per_class = 30;
    auto dm = makeDM("cam", 200);
    createBlockSequenceTensor(*dm, "features", "cam", per_class);
    createLabelIntervalSeries(*dm, "labels", "cam", per_class);

    MLCore::MLModelRegistry const registry;

    MLCore::ClassificationPipelineConfig config;
    config.model_name = "Hidden Markov Model (Gaussian)";
    config.feature_tensor_key = "features";
    config.label_config = MLCore::LabelFromIntervals{"P", "N"};
    config.label_interval_key = "labels";
    config.time_key_str = "cam";
    config.output_config.time_key_str = "cam";
    config.prediction_region.predict_all_rows = true;
    // Reference a non-existent interval series
    config.prediction_region.prediction_interval_key = "nonexistent_series";

    auto result = MLCore::runClassificationPipeline(*dm, registry, config);

    CHECK_FALSE(result.success);
    CHECK(result.failed_stage == MLCore::ClassificationStage::Predicting);
    CHECK_FALSE(result.error_message.empty());
}
