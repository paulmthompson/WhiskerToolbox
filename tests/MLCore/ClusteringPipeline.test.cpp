
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "pipelines/ClusteringPipeline.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/Tensors/TensorData.hpp"
#include "DataManager/Tensors/RowDescriptor.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityRegistry.hpp"
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
 * @brief Create a well-separated 2-cluster feature tensor
 *
 * Cluster 0 (rows 0..per_cluster-1): feature_0 in [0, 5), feature_1 ~ 0
 * Cluster 1 (rows per_cluster..2*per_cluster-1): feature_0 in [10, 15), feature_1 ~ 10
 */
void createClusterTensor(
    DataManager & dm,
    std::string const & key,
    std::string const & time_key,
    std::size_t per_cluster = 50)
{
    auto const total = per_cluster * 2;
    auto const cols = std::size_t{2};
    std::vector<float> data(total * cols);

    for (std::size_t i = 0; i < per_cluster; ++i) {
        data[i * cols + 0] = static_cast<float>(i) * 0.1f;
        data[i * cols + 1] = static_cast<float>(i) * 0.01f;
    }
    for (std::size_t i = 0; i < per_cluster; ++i) {
        auto r = per_cluster + i;
        data[r * cols + 0] = 10.0f + static_cast<float>(i) * 0.1f;
        data[r * cols + 1] = 10.0f + static_cast<float>(i) * 0.01f;
    }

    auto ts = makeDenseTimeStorage(total);
    auto tf = dm.getTime(TimeKey(time_key));

    std::vector<std::string> names = {"feature_0", "feature_1"};
    auto tensor = std::make_shared<TensorData>(
        TensorData::createTimeSeries2D(data, total, cols, ts, tf, names));

    dm.setData<TensorData>(key, tensor, TimeKey(time_key));
}

/**
 * @brief Create a 3-cluster feature tensor for K=3 tests
 *
 * Cluster 0 (rows 0..n-1): near (0, 0)
 * Cluster 1 (rows n..2n-1): near (10, 10)
 * Cluster 2 (rows 2n..3n-1): near (10, 0)
 */
void createThreeClusterTensor(
    DataManager & dm,
    std::string const & key,
    std::string const & time_key,
    std::size_t per_cluster = 30)
{
    auto const total = per_cluster * 3;
    auto const cols = std::size_t{2};
    std::vector<float> data(total * cols);

    // Cluster 0: near (0, 0)
    for (std::size_t i = 0; i < per_cluster; ++i) {
        data[i * cols + 0] = static_cast<float>(i) * 0.05f;
        data[i * cols + 1] = static_cast<float>(i) * 0.05f;
    }
    // Cluster 1: near (10, 10)
    for (std::size_t i = 0; i < per_cluster; ++i) {
        auto r = per_cluster + i;
        data[r * cols + 0] = 10.0f + static_cast<float>(i) * 0.05f;
        data[r * cols + 1] = 10.0f + static_cast<float>(i) * 0.05f;
    }
    // Cluster 2: near (10, 0)
    for (std::size_t i = 0; i < per_cluster; ++i) {
        auto r = 2 * per_cluster + i;
        data[r * cols + 0] = 10.0f + static_cast<float>(i) * 0.05f;
        data[r * cols + 1] = static_cast<float>(i) * 0.05f;
    }

    auto ts = makeDenseTimeStorage(total);
    auto tf = dm.getTime(TimeKey(time_key));

    std::vector<std::string> names = {"feature_0", "feature_1"};
    auto tensor = std::make_shared<TensorData>(
        TensorData::createTimeSeries2D(data, total, cols, ts, tf, names));

    dm.setData<TensorData>(key, tensor, TimeKey(time_key));
}

/**
 * @brief Check cluster assignments are correct with permutation awareness
 *
 * Since cluster IDs are arbitrary, we check that points from the same
 * ground-truth cluster are assigned the same predicted label.
 */
bool clustersAreConsistent(
    arma::Row<std::size_t> const & assignments,
    std::size_t per_cluster,
    std::size_t num_clusters)
{
    // For each ground truth cluster k, check all points get the same assignment
    for (std::size_t k = 0; k < num_clusters; ++k) {
        std::size_t start = k * per_cluster;
        std::size_t expected = assignments[start];
        for (std::size_t i = start + 1; i < start + per_cluster; ++i) {
            if (i < assignments.n_elem && assignments[i] != expected) {
                return false;
            }
        }
    }

    // Also check that different ground-truth clusters get different assignments
    std::vector<std::size_t> cluster_labels;
    for (std::size_t k = 0; k < num_clusters; ++k) {
        cluster_labels.push_back(assignments[k * per_cluster]);
    }
    std::sort(cluster_labels.begin(), cluster_labels.end());
    auto last = std::unique(cluster_labels.begin(), cluster_labels.end());
    return static_cast<std::size_t>(std::distance(cluster_labels.begin(), last)) == num_clusters;
}

} // anonymous namespace

// ============================================================================
// Stage toString
// ============================================================================

TEST_CASE("ClusteringPipeline: stage toString", "[ClusteringPipeline]") {
    CHECK(MLCore::toString(MLCore::ClusteringStage::ValidatingFeatures) == "Validating features");
    CHECK(MLCore::toString(MLCore::ClusteringStage::ConvertingFeatures) == "Converting features");
    CHECK(MLCore::toString(MLCore::ClusteringStage::Fitting) == "Fitting model");
    CHECK(MLCore::toString(MLCore::ClusteringStage::AssigningClusters) == "Assigning clusters");
    CHECK(MLCore::toString(MLCore::ClusteringStage::WritingOutput) == "Writing output");
    CHECK(MLCore::toString(MLCore::ClusteringStage::Complete) == "Complete");
    CHECK(MLCore::toString(MLCore::ClusteringStage::Failed) == "Failed");
}

// ============================================================================
// Error handling — validation failures
// ============================================================================

TEST_CASE("ClusteringPipeline: fails if feature tensor not found",
          "[ClusteringPipeline]")
{
    auto dm = makeDM("cam", 200);
    MLCore::MLModelRegistry registry;

    MLCore::ClusteringPipelineConfig config;
    config.model_name = "K-Means";
    config.feature_tensor_key = "nonexistent";
    config.time_key_str = "cam";

    auto result = MLCore::runClusteringPipeline(*dm, registry, config);
    CHECK_FALSE(result.success);
    CHECK(result.failed_stage == MLCore::ClusteringStage::ValidatingFeatures);
    CHECK(result.error_message.find("not found") != std::string::npos);
}

TEST_CASE("ClusteringPipeline: fails if model not in registry",
          "[ClusteringPipeline]")
{
    auto dm = makeDM("cam", 200);
    createClusterTensor(*dm, "features", "cam", 50);
    MLCore::MLModelRegistry registry;

    MLCore::ClusteringPipelineConfig config;
    config.model_name = "NonexistentModel";
    config.feature_tensor_key = "features";
    config.time_key_str = "cam";

    auto result = MLCore::runClusteringPipeline(*dm, registry, config);
    CHECK_FALSE(result.success);
    CHECK(result.failed_stage == MLCore::ClusteringStage::ValidatingFeatures);
    CHECK(result.error_message.find("not found") != std::string::npos);
}

TEST_CASE("ClusteringPipeline: fails if model is not a clustering model",
          "[ClusteringPipeline]")
{
    auto dm = makeDM("cam", 200);
    createClusterTensor(*dm, "features", "cam", 50);
    MLCore::MLModelRegistry registry;

    // Random Forest is a classification model, not clustering
    MLCore::ClusteringPipelineConfig config;
    config.model_name = "Random Forest";
    config.feature_tensor_key = "features";
    config.time_key_str = "cam";

    auto result = MLCore::runClusteringPipeline(*dm, registry, config);
    CHECK_FALSE(result.success);
    CHECK(result.failed_stage == MLCore::ClusteringStage::ValidatingFeatures);
    CHECK(result.error_message.find("not a clustering model") != std::string::npos);
}

TEST_CASE("ClusteringPipeline: fails if assignment tensor not found",
          "[ClusteringPipeline]")
{
    auto dm = makeDM("cam", 200);
    createClusterTensor(*dm, "features", "cam", 50);
    MLCore::MLModelRegistry registry;

    MLCore::ClusteringPipelineConfig config;
    config.model_name = "K-Means";
    config.feature_tensor_key = "features";
    config.time_key_str = "cam";
    config.assignment_region.assign_all_rows = false;
    config.assignment_region.assignment_tensor_key = "nonexistent_assign";

    auto result = MLCore::runClusteringPipeline(*dm, registry, config);
    CHECK_FALSE(result.success);
    CHECK(result.failed_stage == MLCore::ClusteringStage::AssigningClusters);
    CHECK(result.error_message.find("not found") != std::string::npos);
}

// ============================================================================
// Full pipeline with K-Means
// ============================================================================

TEST_CASE("ClusteringPipeline: end-to-end with K-Means on 2 clusters",
          "[ClusteringPipeline]")
{
    constexpr std::size_t per_cluster = 50;
    auto dm = makeDM("cam", 200);
    createClusterTensor(*dm, "features", "cam", per_cluster);

    MLCore::MLModelRegistry registry;

    MLCore::KMeansParameters km_params;
    km_params.k = 2;
    km_params.seed = 42;

    MLCore::ClusteringPipelineConfig config;
    config.model_name = "K-Means";
    config.model_params = &km_params;
    config.feature_tensor_key = "features";
    config.time_key_str = "cam";
    config.output_config.output_prefix = "Cluster:";
    config.output_config.time_key_str = "cam";
    config.output_config.write_intervals = true;
    config.output_config.write_to_putative_groups = true;
    config.assignment_region.assign_all_rows = true;

    // Track progress stages
    std::vector<MLCore::ClusteringStage> stages_seen;
    auto progress = [&](MLCore::ClusteringStage stage, std::string const &) {
        stages_seen.push_back(stage);
    };

    auto result = MLCore::runClusteringPipeline(*dm, registry, config, progress);

    // Pipeline should succeed
    REQUIRE(result.success);
    CHECK(result.error_message.empty());

    // Fitting info
    CHECK(result.fitting_observations == per_cluster * 2);
    CHECK(result.num_features == 2);
    CHECK(result.num_clusters == 2);
    CHECK(result.rows_dropped_nan == 0);
    CHECK(result.noise_points == 0);

    // Assignment info
    CHECK(result.assignment_observations == per_cluster * 2);
    CHECK(result.cluster_sizes.size() == 2);
    CHECK(result.cluster_sizes[0] + result.cluster_sizes[1] == per_cluster * 2);
    // Each cluster should have exactly per_cluster points
    std::vector<std::size_t> sorted_sizes = result.cluster_sizes;
    std::sort(sorted_sizes.begin(), sorted_sizes.end());
    CHECK(sorted_sizes[0] == per_cluster);
    CHECK(sorted_sizes[1] == per_cluster);

    // Cluster names
    CHECK(result.cluster_names.size() == 2);
    CHECK(result.cluster_names[0] == "Cluster:0");
    CHECK(result.cluster_names[1] == "Cluster:1");

    // Output: interval keys should be present
    CHECK(result.interval_keys.size() == 2);

    // Output: putative groups should be present
    CHECK(result.putative_group_ids.size() == 2);

    // Fitted model should be returned
    REQUIRE(result.fitted_model != nullptr);
    CHECK(result.fitted_model->isTrained());
    CHECK(result.fitted_model->numClasses() == 2);
    CHECK(result.fitted_model->numFeatures() == 2);

    // Verify progress callback was called with expected stages
    REQUIRE(stages_seen.size() >= 4);
    CHECK(stages_seen.front() == MLCore::ClusteringStage::ValidatingFeatures);
    CHECK(stages_seen.back() == MLCore::ClusteringStage::Complete);
}

// ============================================================================
// Full pipeline with K-Means on 3 clusters
// ============================================================================

TEST_CASE("ClusteringPipeline: end-to-end with K-Means on 3 clusters",
          "[ClusteringPipeline]")
{
    constexpr std::size_t per_cluster = 30;
    auto dm = makeDM("cam", 300);
    createThreeClusterTensor(*dm, "features", "cam", per_cluster);

    MLCore::MLModelRegistry registry;

    MLCore::KMeansParameters km_params;
    km_params.k = 3;
    km_params.seed = 42;

    MLCore::ClusteringPipelineConfig config;
    config.model_name = "K-Means";
    config.model_params = &km_params;
    config.feature_tensor_key = "features";
    config.time_key_str = "cam";
    config.output_config.output_prefix = "KM:";
    config.output_config.time_key_str = "cam";
    config.assignment_region.assign_all_rows = true;

    auto result = MLCore::runClusteringPipeline(*dm, registry, config);

    REQUIRE(result.success);
    CHECK(result.num_clusters == 3);
    CHECK(result.fitting_observations == per_cluster * 3);
    CHECK(result.assignment_observations == per_cluster * 3);
    CHECK(result.cluster_sizes.size() == 3);

    // All clusters should be non-empty for well-separated data
    for (auto size : result.cluster_sizes) {
        CHECK(size > 0);
    }

    // Total assigned should equal total observations
    std::size_t total_assigned = 0;
    for (auto size : result.cluster_sizes) {
        total_assigned += size;
    }
    CHECK(total_assigned == per_cluster * 3);

    // Check cluster names
    CHECK(result.cluster_names.size() == 3);
    CHECK(result.cluster_names[0] == "KM:0");
    CHECK(result.cluster_names[1] == "KM:1");
    CHECK(result.cluster_names[2] == "KM:2");

    REQUIRE(result.fitted_model != nullptr);
    CHECK(result.fitted_model->getName() == "K-Means");
}

// ============================================================================
// Full pipeline with DBSCAN
// ============================================================================

TEST_CASE("ClusteringPipeline: end-to-end with DBSCAN",
          "[ClusteringPipeline]")
{
    constexpr std::size_t per_cluster = 50;
    auto dm = makeDM("cam", 200);
    createClusterTensor(*dm, "features", "cam", per_cluster);

    MLCore::MLModelRegistry registry;

    MLCore::DBSCANParameters dbscan_params;
    dbscan_params.epsilon = 2.0;
    dbscan_params.min_points = 3;

    MLCore::ClusteringPipelineConfig config;
    config.model_name = "DBSCAN";
    config.model_params = &dbscan_params;
    config.feature_tensor_key = "features";
    config.time_key_str = "cam";
    config.output_config.output_prefix = "DB:";
    config.output_config.time_key_str = "cam";
    config.assignment_region.assign_all_rows = true;

    auto result = MLCore::runClusteringPipeline(*dm, registry, config);

    REQUIRE(result.success);
    // DBSCAN should discover 2 clusters for well-separated data
    CHECK(result.num_clusters == 2);
    CHECK(result.fitting_observations == per_cluster * 2);
    CHECK(result.assignment_observations == per_cluster * 2);

    // Cluster names use the configured prefix
    for (auto const & name : result.cluster_names) {
        CHECK(name.find("DB:") == 0);
    }

    REQUIRE(result.fitted_model != nullptr);
    CHECK(result.fitted_model->getName() == "DBSCAN");
}

// ============================================================================
// Full pipeline with Gaussian Mixture Model
// ============================================================================

TEST_CASE("ClusteringPipeline: end-to-end with GMM",
          "[ClusteringPipeline]")
{
    constexpr std::size_t per_cluster = 50;
    auto dm = makeDM("cam", 200);
    createClusterTensor(*dm, "features", "cam", per_cluster);

    MLCore::MLModelRegistry registry;

    MLCore::GMMParameters gmm_params;
    gmm_params.k = 2;
    gmm_params.seed = 42;

    MLCore::ClusteringPipelineConfig config;
    config.model_name = "Gaussian Mixture Model";
    config.model_params = &gmm_params;
    config.feature_tensor_key = "features";
    config.time_key_str = "cam";
    config.output_config.output_prefix = "GMM:";
    config.output_config.time_key_str = "cam";
    config.output_config.write_probabilities = true;
    config.assignment_region.assign_all_rows = true;

    auto result = MLCore::runClusteringPipeline(*dm, registry, config);

    REQUIRE(result.success);
    CHECK(result.num_clusters == 2);
    CHECK(result.fitting_observations == per_cluster * 2);
    CHECK(result.assignment_observations == per_cluster * 2);

    // GMM supports probability output — check probability keys are present
    // (GMM doesn't use predictProbabilities() on the base interface, so
    // this depends on the implementation detail; probability_keys may be empty
    // if GMM doesn't override predictProbabilities())
    // We still check the pipeline runs successfully either way

    REQUIRE(result.fitted_model != nullptr);
    CHECK(result.fitted_model->getName() == "Gaussian Mixture Model");
}

// ============================================================================
// Pipeline with z-score normalization
// ============================================================================

TEST_CASE("ClusteringPipeline: z-score normalization",
          "[ClusteringPipeline]")
{
    constexpr std::size_t per_cluster = 50;
    auto dm = makeDM("cam", 200);
    createClusterTensor(*dm, "features", "cam", per_cluster);

    MLCore::MLModelRegistry registry;

    MLCore::KMeansParameters km_params;
    km_params.k = 2;
    km_params.seed = 42;

    MLCore::ClusteringPipelineConfig config;
    config.model_name = "K-Means";
    config.model_params = &km_params;
    config.feature_tensor_key = "features";
    config.time_key_str = "cam";
    config.conversion_config.zscore_normalize = true;
    config.output_config.time_key_str = "cam";
    config.assignment_region.assign_all_rows = true;

    auto result = MLCore::runClusteringPipeline(*dm, registry, config);

    REQUIRE(result.success);
    CHECK(result.num_features == 2);
    CHECK(result.num_clusters == 2);
    // Well-separated clusters should still be found after z-score
    CHECK(result.fitting_observations == per_cluster * 2);
}

// ============================================================================
// Pipeline with NaN rows in features
// ============================================================================

TEST_CASE("ClusteringPipeline: handles NaN rows in features",
          "[ClusteringPipeline]")
{
    auto dm = makeDM("cam", 200);

    // Create tensor with some NaN rows
    std::size_t total = 100;
    std::size_t cols = 2;
    std::vector<float> data(total * cols);
    for (std::size_t i = 0; i < 50; ++i) {
        data[i * cols + 0] = static_cast<float>(i) * 0.1f;
        data[i * cols + 1] = static_cast<float>(i) * 0.01f;
    }
    for (std::size_t i = 50; i < total; ++i) {
        data[i * cols + 0] = 10.0f + static_cast<float>(i - 50) * 0.1f;
        data[i * cols + 1] = 10.0f + static_cast<float>(i - 50) * 0.01f;
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

    MLCore::MLModelRegistry registry;

    MLCore::KMeansParameters km_params;
    km_params.k = 2;
    km_params.seed = 42;

    MLCore::ClusteringPipelineConfig config;
    config.model_name = "K-Means";
    config.model_params = &km_params;
    config.feature_tensor_key = "features";
    config.time_key_str = "cam";
    config.conversion_config.drop_nan = true;
    config.output_config.time_key_str = "cam";
    config.assignment_region.assign_all_rows = true;

    auto result = MLCore::runClusteringPipeline(*dm, registry, config);

    REQUIRE(result.success);
    CHECK(result.rows_dropped_nan == 2);
    CHECK(result.fitting_observations == 98);
    CHECK(result.assignment_observations == 98);
}

// ============================================================================
// Pipeline with no assignment (fit only)
// ============================================================================

TEST_CASE("ClusteringPipeline: fit only, no assignment",
          "[ClusteringPipeline]")
{
    constexpr std::size_t per_cluster = 50;
    auto dm = makeDM("cam", 200);
    createClusterTensor(*dm, "features", "cam", per_cluster);

    MLCore::MLModelRegistry registry;

    MLCore::KMeansParameters km_params;
    km_params.k = 2;
    km_params.seed = 42;

    MLCore::ClusteringPipelineConfig config;
    config.model_name = "K-Means";
    config.model_params = &km_params;
    config.feature_tensor_key = "features";
    config.time_key_str = "cam";
    config.assignment_region.assign_all_rows = false;
    // No assignment_tensor_key → no assignment
    config.output_config.time_key_str = "cam";

    auto result = MLCore::runClusteringPipeline(*dm, registry, config);

    REQUIRE(result.success);
    CHECK(result.fitting_observations == per_cluster * 2);
    CHECK(result.assignment_observations == 0);
    CHECK(result.cluster_sizes.empty());
    CHECK(result.interval_keys.empty());
    CHECK(result.putative_group_ids.empty());

    // Model should still be fitted and returned
    REQUIRE(result.fitted_model != nullptr);
    CHECK(result.fitted_model->isTrained());
}

// ============================================================================
// Pipeline with separate assignment tensor
// ============================================================================

TEST_CASE("ClusteringPipeline: assign on separate tensor",
          "[ClusteringPipeline]")
{
    constexpr std::size_t per_cluster = 50;
    auto dm = makeDM("cam", 300);
    createClusterTensor(*dm, "fit_features", "cam", per_cluster);

    // Create a separate assignment tensor with features in the same 2-cluster pattern
    std::size_t assign_rows = 20;
    std::size_t cols = 2;
    std::vector<float> assign_data(assign_rows * cols);
    for (std::size_t i = 0; i < assign_rows; ++i) {
        if (i % 2 == 0) {
            assign_data[i * cols + 0] = 1.0f;   // near cluster 0
            assign_data[i * cols + 1] = 0.5f;
        } else {
            assign_data[i * cols + 0] = 12.0f;  // near cluster 1
            assign_data[i * cols + 1] = 11.0f;
        }
    }

    auto assign_times = std::vector<TimeFrameIndex>();
    assign_times.reserve(assign_rows);
    for (std::size_t i = 0; i < assign_rows; ++i) {
        assign_times.emplace_back(static_cast<int64_t>(200 + i));
    }
    auto assign_ts = TimeIndexStorageFactory::createFromTimeIndices(assign_times);
    auto tf = dm->getTime(TimeKey("cam"));
    auto assign_tensor = std::make_shared<TensorData>(
        TensorData::createTimeSeries2D(assign_data, assign_rows, cols,
                                        assign_ts, tf, {"feature_0", "feature_1"}));
    dm->setData<TensorData>("assign_features", assign_tensor, TimeKey("cam"));

    MLCore::MLModelRegistry registry;

    MLCore::KMeansParameters km_params;
    km_params.k = 2;
    km_params.seed = 42;

    MLCore::ClusteringPipelineConfig config;
    config.model_name = "K-Means";
    config.model_params = &km_params;
    config.feature_tensor_key = "fit_features";
    config.time_key_str = "cam";
    config.assignment_region.assign_all_rows = false;
    config.assignment_region.assignment_tensor_key = "assign_features";
    config.output_config.output_prefix = "SepCluster:";
    config.output_config.time_key_str = "cam";
    config.output_config.write_intervals = true;

    auto result = MLCore::runClusteringPipeline(*dm, registry, config);

    REQUIRE(result.success);
    CHECK(result.fitting_observations == per_cluster * 2);
    CHECK(result.assignment_observations == assign_rows);

    // Interval keys should be present
    CHECK(result.interval_keys.size() == 2);

    // Interval keys should use configured prefix
    for (auto const & key : result.interval_keys) {
        CHECK(key.find("SepCluster:") == 0);
    }
}

// ============================================================================
// Pipeline result carries fitted model for reuse
// ============================================================================

TEST_CASE("ClusteringPipeline: returned model can re-assign clusters",
          "[ClusteringPipeline]")
{
    constexpr std::size_t per_cluster = 50;
    auto dm = makeDM("cam", 200);
    createClusterTensor(*dm, "features", "cam", per_cluster);

    MLCore::MLModelRegistry registry;

    MLCore::KMeansParameters km_params;
    km_params.k = 2;
    km_params.seed = 42;

    MLCore::ClusteringPipelineConfig config;
    config.model_name = "K-Means";
    config.model_params = &km_params;
    config.feature_tensor_key = "features";
    config.time_key_str = "cam";
    config.assignment_region.assign_all_rows = false;
    config.output_config.time_key_str = "cam";

    auto result = MLCore::runClusteringPipeline(*dm, registry, config);
    REQUIRE(result.success);
    REQUIRE(result.fitted_model != nullptr);

    // Use the model to assign new data
    arma::mat test_features(2, 4);
    // Two points near cluster 0 and two near cluster 1
    test_features.col(0) = arma::vec{0.5, 0.0};
    test_features.col(1) = arma::vec{1.0, 0.0};
    test_features.col(2) = arma::vec{12.0, 12.0};
    test_features.col(3) = arma::vec{13.0, 13.0};

    arma::Row<std::size_t> assignments;
    bool ok = result.fitted_model->assignClusters(test_features, assignments);
    REQUIRE(ok);
    REQUIRE(assignments.n_elem == 4);

    // Points near cluster 0 should get the same assignment
    CHECK(assignments[0] == assignments[1]);
    // Points near cluster 1 should get the same assignment
    CHECK(assignments[2] == assignments[3]);
    // The two groups should be different clusters
    CHECK(assignments[0] != assignments[2]);
}

// ============================================================================
// Pipeline: progress callback receives all stages
// ============================================================================

TEST_CASE("ClusteringPipeline: progress callback receives all stages",
          "[ClusteringPipeline]")
{
    constexpr std::size_t per_cluster = 30;
    auto dm = makeDM("cam", 200);
    createClusterTensor(*dm, "features", "cam", per_cluster);

    MLCore::MLModelRegistry registry;

    MLCore::KMeansParameters km_params;
    km_params.k = 2;
    km_params.seed = 42;

    MLCore::ClusteringPipelineConfig config;
    config.model_name = "K-Means";
    config.model_params = &km_params;
    config.feature_tensor_key = "features";
    config.time_key_str = "cam";
    config.output_config.time_key_str = "cam";
    config.assignment_region.assign_all_rows = true;

    std::vector<MLCore::ClusteringStage> stages;
    std::vector<std::string> messages;
    auto progress = [&](MLCore::ClusteringStage stage, std::string const & msg) {
        stages.push_back(stage);
        messages.push_back(msg);
    };

    auto result = MLCore::runClusteringPipeline(*dm, registry, config, progress);
    REQUIRE(result.success);

    // Should see stages in order
    REQUIRE(stages.size() >= 5);
    CHECK(stages[0] == MLCore::ClusteringStage::ValidatingFeatures);
    CHECK(stages[1] == MLCore::ClusteringStage::ConvertingFeatures);
    CHECK(stages[2] == MLCore::ClusteringStage::Fitting);
    CHECK(stages[3] == MLCore::ClusteringStage::AssigningClusters);
    CHECK(stages[4] == MLCore::ClusteringStage::WritingOutput);

    // Final stage should be Complete
    CHECK(stages.back() == MLCore::ClusteringStage::Complete);

    // All messages should be non-empty
    for (auto const & msg : messages) {
        CHECK_FALSE(msg.empty());
    }
}

// ============================================================================
// Pipeline: output prefix naming
// ============================================================================

TEST_CASE("ClusteringPipeline: output uses configured prefix",
          "[ClusteringPipeline]")
{
    constexpr std::size_t per_cluster = 30;
    auto dm = makeDM("cam", 200);
    createClusterTensor(*dm, "features", "cam", per_cluster);

    MLCore::MLModelRegistry registry;

    MLCore::KMeansParameters km_params;
    km_params.k = 2;
    km_params.seed = 42;

    MLCore::ClusteringPipelineConfig config;
    config.model_name = "K-Means";
    config.model_params = &km_params;
    config.feature_tensor_key = "features";
    config.time_key_str = "cam";
    config.output_config.output_prefix = "MyCluster:";
    config.output_config.time_key_str = "cam";
    config.assignment_region.assign_all_rows = true;

    auto result = MLCore::runClusteringPipeline(*dm, registry, config);

    REQUIRE(result.success);

    // Interval keys should start with the configured prefix
    for (auto const & key : result.interval_keys) {
        CHECK(key.find("MyCluster:") == 0);
    }

    // Cluster names should use the prefix
    for (auto const & name : result.cluster_names) {
        CHECK(name.find("MyCluster:") == 0);
    }
}

// ============================================================================
// Pipeline: default parameters (no model_params)
// ============================================================================

TEST_CASE("ClusteringPipeline: works with default parameters (nullptr)",
          "[ClusteringPipeline]")
{
    constexpr std::size_t per_cluster = 50;
    auto dm = makeDM("cam", 200);
    createClusterTensor(*dm, "features", "cam", per_cluster);

    MLCore::MLModelRegistry registry;

    MLCore::ClusteringPipelineConfig config;
    config.model_name = "K-Means";
    config.model_params = nullptr;  // use defaults
    config.feature_tensor_key = "features";
    config.time_key_str = "cam";
    config.output_config.time_key_str = "cam";
    config.assignment_region.assign_all_rows = true;

    auto result = MLCore::runClusteringPipeline(*dm, registry, config);

    REQUIRE(result.success);
    // Default K-Means has k=3, so should get 3 clusters
    CHECK(result.num_clusters == 3);
    CHECK(result.fitting_observations == per_cluster * 2);
    REQUIRE(result.fitted_model != nullptr);
    CHECK(result.fitted_model->isTrained());
}
