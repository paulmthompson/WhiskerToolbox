
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "pipelines/ClassificationPipeline.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/Tensors/TensorData.hpp"
#include "DataManager/Tensors/RowDescriptor.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityRegistry.hpp"
#include "Entity/EntityTypes.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"

#include "models/MLModelRegistry.hpp"
#include "models/MLModelParameters.hpp"
#include "models/MLTaskType.hpp"

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
    std::size_t per_class = 50)
{
    auto const total = per_class * 2;
    auto const cols = std::size_t{2};
    std::vector<float> data(total * cols);

    // Feature 0: separable, Feature 1: noise
    for (std::size_t i = 0; i < per_class; ++i) {
        data[i * cols + 0] = static_cast<float>(i) * 0.1f;          // class 0: low
        data[i * cols + 1] = static_cast<float>(i) * 0.01f;         // noise
    }
    for (std::size_t i = 0; i < per_class; ++i) {
        auto r = per_class + i;
        data[r * cols + 0] = 10.0f + static_cast<float>(i) * 0.1f;  // class 1: high
        data[r * cols + 1] = static_cast<float>(i) * 0.01f;          // noise
    }

    auto ts = makeDenseTimeStorage(total);
    auto tf = dm.getTime(TimeKey(time_key));

    std::vector<std::string> names = {"feature_0", "feature_1"};
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
    std::size_t per_class = 50)
{
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
    std::size_t per_class = 50)
{
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

} // anonymous namespace

// ============================================================================
// Stage toString
// ============================================================================

TEST_CASE("ClassificationPipeline: stage toString", "[ClassificationPipeline]") {
    CHECK(MLCore::toString(MLCore::ClassificationStage::ValidatingFeatures) == "Validating features");
    CHECK(MLCore::toString(MLCore::ClassificationStage::ConvertingFeatures) == "Converting features");
    CHECK(MLCore::toString(MLCore::ClassificationStage::AssemblingLabels) == "Assembling labels");
    CHECK(MLCore::toString(MLCore::ClassificationStage::BalancingClasses) == "Balancing classes");
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
          "[ClassificationPipeline]")
{
    auto dm = makeDM("cam", 200);
    MLCore::MLModelRegistry registry;

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
          "[ClassificationPipeline]")
{
    auto dm = makeDM("cam", 200);
    createLinSepTensor(*dm, "features", "cam", 50);
    MLCore::MLModelRegistry registry;

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
          "[ClassificationPipeline]")
{
    auto dm = makeDM("cam", 200);
    createLinSepTensor(*dm, "features", "cam", 50);
    MLCore::MLModelRegistry registry;

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
          "[ClassificationPipeline]")
{
    constexpr std::size_t per_class = 50;
    auto dm = makeDM("cam", 200);
    createLinSepTensor(*dm, "features", "cam", per_class);
    createLabelIntervalSeries(*dm, "labels", "cam", per_class);

    MLCore::MLModelRegistry registry;

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
    CHECK(bm.accuracy >= 0.90);  // Linearly separable → should get high accuracy

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
          "[ClassificationPipeline]")
{
    constexpr std::size_t per_class = 50;
    auto dm = makeDM("cam", 200);
    createLinSepTensor(*dm, "features", "cam", per_class);
    createLabelIntervalSeries(*dm, "labels", "cam", per_class);

    MLCore::MLModelRegistry registry;

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
          "[ClassificationPipeline]")
{
    constexpr std::size_t per_class = 50;
    auto dm = makeDM("cam", 200);
    createLinSepTensor(*dm, "features", "cam", per_class);
    createLabelIntervalSeries(*dm, "labels", "cam", per_class);

    MLCore::MLModelRegistry registry;

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
          "[ClassificationPipeline]")
{
    // Create imbalanced data: 80 vs 20
    auto dm = makeDM("cam", 200);

    // Build imbalanced tensor manually
    std::size_t class0 = 80;
    std::size_t class1 = 20;
    std::size_t total = class0 + class1;
    std::size_t cols = 2;
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

    MLCore::MLModelRegistry registry;

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
          "[ClassificationPipeline]")
{
    constexpr std::size_t per_class = 50;
    auto dm = makeDM("cam", 200);
    createLinSepTensor(*dm, "features", "cam", per_class);
    auto [g0, g1] = createTimeEntityGroups(*dm, "cam", per_class);

    MLCore::MLModelRegistry registry;

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
          "[ClassificationPipeline]")
{
    constexpr std::size_t per_class = 50;
    auto dm = makeDM("cam", 200);
    createLinSepTensor(*dm, "features", "cam", per_class);
    createLabelIntervalSeries(*dm, "labels", "cam", per_class);

    MLCore::MLModelRegistry registry;

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
          "[ClassificationPipeline]")
{
    auto dm = makeDM("cam", 200);

    // Create tensor with some NaN rows
    std::size_t total = 100;
    std::size_t cols = 2;
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

    MLCore::MLModelRegistry registry;

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
          "[ClassificationPipeline]")
{
    constexpr std::size_t per_class = 50;
    auto dm = makeDM("cam", 200);
    createLinSepTensor(*dm, "features", "cam", per_class);
    createLabelIntervalSeries(*dm, "labels", "cam", per_class);

    MLCore::MLModelRegistry registry;

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
          "[ClassificationPipeline]")
{
    constexpr std::size_t per_class = 50;
    auto dm = makeDM("cam", 300);
    createLinSepTensor(*dm, "train_features", "cam", per_class);
    createLabelIntervalSeries(*dm, "labels", "cam", per_class);

    // Create a separate prediction tensor with features in the same range
    std::size_t pred_rows = 20;
    std::size_t cols = 2;
    std::vector<float> pred_data(pred_rows * cols);
    for (std::size_t i = 0; i < pred_rows; ++i) {
        // Alternate between class 0 and class 1 ranges
        if (i % 2 == 0) {
            pred_data[i * cols + 0] = 1.0f;   // class 0 range
        } else {
            pred_data[i * cols + 0] = 12.0f;  // class 1 range
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

    MLCore::MLModelRegistry registry;

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
          "[ClassificationPipeline]")
{
    constexpr std::size_t per_class = 50;
    auto dm = makeDM("cam", 200);
    createLinSepTensor(*dm, "features", "cam", per_class);
    createLabelIntervalSeries(*dm, "labels", "cam", per_class);

    MLCore::MLModelRegistry registry;

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
          "[ClassificationPipeline]")
{
    constexpr std::size_t per_class = 50;
    auto dm = makeDM("cam", 200);
    createLinSepTensor(*dm, "features", "cam", per_class);
    createLabelIntervalSeries(*dm, "labels", "cam", per_class);

    MLCore::MLModelRegistry registry;

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
    bool ok = result.trained_model->predict(test_features, preds);
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
          "[ClassificationPipeline]")
{
    constexpr std::size_t per_class = 30;
    auto dm = makeDM("cam", 200);
    createLinSepTensor(*dm, "features", "cam", per_class);
    createLabelIntervalSeries(*dm, "labels", "cam", per_class);

    MLCore::MLModelRegistry registry;

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
    for (auto const & msg : messages) {
        CHECK_FALSE(msg.empty());
    }
}

// ============================================================================
// Pipeline: output prefix naming
// ============================================================================

TEST_CASE("ClassificationPipeline: output uses configured prefix",
          "[ClassificationPipeline]")
{
    constexpr std::size_t per_class = 30;
    auto dm = makeDM("cam", 200);
    createLinSepTensor(*dm, "features", "cam", per_class);
    createLabelIntervalSeries(*dm, "labels", "cam", per_class);

    MLCore::MLModelRegistry registry;

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
    for (auto const & key : result.writer_result->interval_keys) {
        CHECK(key.find("MyPrefix:") == 0);
    }
}
