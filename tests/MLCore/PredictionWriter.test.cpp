
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "output/PredictionWriter.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityRegistry.hpp"
#include "Entity/EntityTypes.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <algorithm>
#include <armadillo>
#include <cstddef>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

using Catch::Matchers::WithinRel;

// ============================================================================
// Helpers
// ============================================================================

namespace {

/**
 * @brief Create a simple identity TimeFrame (index i → time i)
 */
std::shared_ptr<TimeFrame> makeTimeFrame(int count) {
    std::vector<int> times(count);
    for (int i = 0; i < count; ++i) {
        times[i] = i;
    }
    return std::make_shared<TimeFrame>(times);
}

/**
 * @brief Build a vector of TimeFrameIndex from int values
 */
std::vector<TimeFrameIndex> makeRowTimes(std::vector<int64_t> const & values) {
    std::vector<TimeFrameIndex> times;
    times.reserve(values.size());
    for (auto v : values) {
        times.emplace_back(v);
    }
    return times;
}

/**
 * @brief Create a DataManager with a TimeFrame registered under given key
 */
std::shared_ptr<DataManager> makeDM(std::string const & time_key, int frame_count) {
    auto dm = std::make_shared<DataManager>();
    dm->setTime(TimeKey(time_key), makeTimeFrame(frame_count));
    return dm;
}

/**
 * @brief Make a simple binary PredictionOutput
 *
 * Creates predictions for times [0..n), alternating between classes
 * based on the provided label pattern.
 */
MLCore::PredictionOutput makeBinaryPrediction(
    std::vector<std::size_t> const & labels)
{
    MLCore::PredictionOutput output;
    output.class_predictions = arma::Row<std::size_t>(labels.size());
    for (std::size_t i = 0; i < labels.size(); ++i) {
        output.class_predictions[i] = labels[i];
    }
    output.prediction_times = makeRowTimes(
        [&]() {
            std::vector<int64_t> v(labels.size());
            std::iota(v.begin(), v.end(), 0);
            return v;
        }());
    return output;
}

/**
 * @brief Collect all intervals from a DigitalIntervalSeries into a vector
 */
std::vector<std::pair<int64_t, int64_t>> collectIntervals(
    DigitalIntervalSeries const & series)
{
    std::vector<std::pair<int64_t, int64_t>> result;
    for (auto iv : series.view()) {
        result.emplace_back(iv.interval.start, iv.interval.end);
    }
    return result;
}

} // anonymous namespace

// ============================================================================
// writePredictionsAsIntervals
// ============================================================================

TEST_CASE("writePredictionsAsIntervals: basic binary contiguous", "[PredictionWriter]") {
    // Predictions: 0 0 0 1 1 1 0 0
    // Expected class 0: [0,2], [6,7]
    // Expected class 1: [3,5]
    auto dm = makeDM("cam", 10);
    auto output = makeBinaryPrediction({0, 0, 0, 1, 1, 1, 0, 0});

    MLCore::PredictionWriterConfig config;
    config.time_key_str = "cam";
    config.output_prefix = "Pred:";

    auto keys = MLCore::writePredictionsAsIntervals(*dm, output, {"NoContact", "Contact"}, config);

    REQUIRE(keys.size() == 2);
    REQUIRE(keys[0] == "Pred:NoContact");
    REQUIRE(keys[1] == "Pred:Contact");

    // Verify class 0 intervals
    auto no_contact = dm->getData<DigitalIntervalSeries>("Pred:NoContact");
    REQUIRE(no_contact != nullptr);
    auto no_contact_ivs = collectIntervals(*no_contact);
    REQUIRE(no_contact_ivs.size() == 2);
    CHECK(no_contact_ivs[0] == std::pair<int64_t, int64_t>{0, 2});
    CHECK(no_contact_ivs[1] == std::pair<int64_t, int64_t>{6, 7});

    // Verify class 1 intervals
    auto contact = dm->getData<DigitalIntervalSeries>("Pred:Contact");
    REQUIRE(contact != nullptr);
    auto contact_ivs = collectIntervals(*contact);
    REQUIRE(contact_ivs.size() == 1);
    CHECK(contact_ivs[0] == std::pair<int64_t, int64_t>{3, 5});
}

TEST_CASE("writePredictionsAsIntervals: single class all same", "[PredictionWriter]") {
    auto dm = makeDM("cam", 10);
    auto output = makeBinaryPrediction({1, 1, 1, 1, 1});

    MLCore::PredictionWriterConfig config;
    config.time_key_str = "cam";

    auto keys = MLCore::writePredictionsAsIntervals(*dm, output, {"A", "B"}, config);

    REQUIRE(keys.size() == 2);

    // Class A should have no intervals
    auto a_series = dm->getData<DigitalIntervalSeries>(keys[0]);
    REQUIRE(a_series != nullptr);
    CHECK(a_series->size() == 0);

    // Class B should have one interval [0, 4]
    auto b_series = dm->getData<DigitalIntervalSeries>(keys[1]);
    REQUIRE(b_series != nullptr);
    auto b_ivs = collectIntervals(*b_series);
    REQUIRE(b_ivs.size() == 1);
    CHECK(b_ivs[0] == std::pair<int64_t, int64_t>{0, 4});
}

TEST_CASE("writePredictionsAsIntervals: non-contiguous times", "[PredictionWriter]") {
    // Times: 0, 1, 5, 6, 10 — gaps between groups
    // Labels: 0, 0, 0, 0, 0
    // Expected: [0,1], [5,6], [10,10] (gaps break intervals)
    auto dm = makeDM("cam", 20);

    MLCore::PredictionOutput output;
    output.class_predictions = {0, 0, 0, 0, 0};
    output.prediction_times = makeRowTimes({0, 1, 5, 6, 10});

    MLCore::PredictionWriterConfig config;
    config.time_key_str = "cam";

    auto keys = MLCore::writePredictionsAsIntervals(*dm, output, {"All"}, config);
    REQUIRE(keys.size() == 1);

    auto series = dm->getData<DigitalIntervalSeries>(keys[0]);
    REQUIRE(series != nullptr);
    auto ivs = collectIntervals(*series);
    REQUIRE(ivs.size() == 3);
    CHECK(ivs[0] == std::pair<int64_t, int64_t>{0, 1});
    CHECK(ivs[1] == std::pair<int64_t, int64_t>{5, 6});
    CHECK(ivs[2] == std::pair<int64_t, int64_t>{10, 10});
}

TEST_CASE("writePredictionsAsIntervals: three classes", "[PredictionWriter]") {
    auto dm = makeDM("cam", 20);

    MLCore::PredictionOutput output;
    output.class_predictions = {0, 1, 2, 2, 1, 0};
    output.prediction_times = makeRowTimes({0, 1, 2, 3, 4, 5});

    auto keys = MLCore::writePredictionsAsIntervals(
        *dm, output, {"A", "B", "C"});

    REQUIRE(keys.size() == 3);

    auto a = dm->getData<DigitalIntervalSeries>(keys[0]);
    auto a_ivs = collectIntervals(*a);
    // Class A: times 0 and 5 (non-contiguous)
    REQUIRE(a_ivs.size() == 2);
    CHECK(a_ivs[0] == std::pair<int64_t, int64_t>{0, 0});
    CHECK(a_ivs[1] == std::pair<int64_t, int64_t>{5, 5});

    auto b = dm->getData<DigitalIntervalSeries>(keys[1]);
    auto b_ivs = collectIntervals(*b);
    // Class B: times 1 and 4 (non-contiguous)
    REQUIRE(b_ivs.size() == 2);
    CHECK(b_ivs[0] == std::pair<int64_t, int64_t>{1, 1});
    CHECK(b_ivs[1] == std::pair<int64_t, int64_t>{4, 4});

    auto c = dm->getData<DigitalIntervalSeries>(keys[2]);
    auto c_ivs = collectIntervals(*c);
    // Class C: times 2-3 (contiguous)
    REQUIRE(c_ivs.size() == 1);
    CHECK(c_ivs[0] == std::pair<int64_t, int64_t>{2, 3});
}

// ============================================================================
// writeProbabilitiesAsAnalog
// ============================================================================

TEST_CASE("writeProbabilitiesAsAnalog: basic binary", "[PredictionWriter]") {
    auto dm = makeDM("cam", 10);

    MLCore::PredictionOutput output;
    output.class_predictions = {0, 0, 1, 1};
    output.prediction_times = makeRowTimes({0, 1, 2, 3});

    // 2 classes × 4 observations
    arma::mat probs(2, 4);
    probs(0, 0) = 0.9; probs(1, 0) = 0.1;
    probs(0, 1) = 0.8; probs(1, 1) = 0.2;
    probs(0, 2) = 0.3; probs(1, 2) = 0.7;
    probs(0, 3) = 0.1; probs(1, 3) = 0.9;
    output.class_probabilities = probs;

    MLCore::PredictionWriterConfig config;
    config.time_key_str = "cam";
    config.output_prefix = "ML:";

    auto keys = MLCore::writeProbabilitiesAsAnalog(*dm, output, {"No", "Yes"}, config);

    REQUIRE(keys.size() == 2);
    REQUIRE(keys[0] == "ML:prob_No");
    REQUIRE(keys[1] == "ML:prob_Yes");

    // Verify analog series for class 0
    auto no_prob = dm->getData<AnalogTimeSeries>("ML:prob_No");
    REQUIRE(no_prob != nullptr);

    // Collect values via view
    std::vector<float> no_values;
    for (auto tvp : no_prob->view()) {
        no_values.push_back(tvp.value());
    }
    REQUIRE(no_values.size() == 4);
    CHECK_THAT(static_cast<double>(no_values[0]), WithinRel(0.9, 1e-5));
    CHECK_THAT(static_cast<double>(no_values[1]), WithinRel(0.8, 1e-5));
    CHECK_THAT(static_cast<double>(no_values[2]), WithinRel(0.3, 1e-5));
    CHECK_THAT(static_cast<double>(no_values[3]), WithinRel(0.1, 1e-5));

    // Verify analog series for class 1
    auto yes_prob = dm->getData<AnalogTimeSeries>("ML:prob_Yes");
    REQUIRE(yes_prob != nullptr);

    std::vector<float> yes_values;
    for (auto tvp : yes_prob->view()) {
        yes_values.push_back(tvp.value());
    }
    REQUIRE(yes_values.size() == 4);
    CHECK_THAT(static_cast<double>(yes_values[0]), WithinRel(0.1, 1e-5));
    CHECK_THAT(static_cast<double>(yes_values[3]), WithinRel(0.9, 1e-5));
}

TEST_CASE("writeProbabilitiesAsAnalog: no probabilities returns empty", "[PredictionWriter]") {
    auto dm = makeDM("cam", 10);

    MLCore::PredictionOutput output;
    output.class_predictions = {0, 1};
    output.prediction_times = makeRowTimes({0, 1});
    // class_probabilities left as std::nullopt

    auto keys = MLCore::writeProbabilitiesAsAnalog(*dm, output, {"A", "B"});

    CHECK(keys.empty());
}

// ============================================================================
// writeTimePredictionsToGroups
// ============================================================================

TEST_CASE("writeTimePredictionsToGroups: basic binary", "[PredictionWriter]") {
    auto dm = makeDM("cam", 10);

    MLCore::PredictionOutput output;
    output.class_predictions = {0, 0, 1, 1, 0};
    output.prediction_times = makeRowTimes({0, 1, 2, 3, 4});

    MLCore::PredictionWriterConfig config;
    config.time_key_str = "cam";
    config.output_prefix = "Predicted:";

    auto group_ids = MLCore::writeTimePredictionsToGroups(
        *dm, output, {"Outside", "Inside"}, config);

    REQUIRE(group_ids.size() == 2);

    auto * groups = dm->getEntityGroupManager();
    REQUIRE(groups != nullptr);

    // Check group names
    auto desc0 = groups->getGroupDescriptor(group_ids[0]);
    REQUIRE(desc0.has_value());
    CHECK(desc0->name == "Predicted:Outside");

    auto desc1 = groups->getGroupDescriptor(group_ids[1]);
    REQUIRE(desc1.has_value());
    CHECK(desc1->name == "Predicted:Inside");

    // Check entity counts
    CHECK(groups->getGroupSize(group_ids[0]) == 3); // times 0,1,4
    CHECK(groups->getGroupSize(group_ids[1]) == 2); // times 2,3
}

TEST_CASE("writeTimePredictionsToGroups: entities are valid TimeEntities", "[PredictionWriter]") {
    auto dm = makeDM("cam", 10);

    MLCore::PredictionOutput output;
    output.class_predictions = {0, 1};
    output.prediction_times = makeRowTimes({5, 7});

    MLCore::PredictionWriterConfig config;
    config.time_key_str = "cam";

    auto group_ids = MLCore::writeTimePredictionsToGroups(
        *dm, output, {"A", "B"}, config);

    auto * groups = dm->getEntityGroupManager();
    auto * registry = dm->getEntityRegistry();

    // Verify entity in group A is the TimeEntity for time 5
    auto entities_a = groups->getEntitiesInGroup(group_ids[0]);
    REQUIRE(entities_a.size() == 1);

    auto desc = registry->get(entities_a[0]);
    REQUIRE(desc.has_value());
    CHECK(desc->kind == EntityKind::TimeEntity);
    CHECK(desc->data_key == "cam");
    CHECK(desc->time_value == 5);

    // Verify entity in group B is the TimeEntity for time 7
    auto entities_b = groups->getEntitiesInGroup(group_ids[1]);
    REQUIRE(entities_b.size() == 1);

    auto desc_b = registry->get(entities_b[0]);
    REQUIRE(desc_b.has_value());
    CHECK(desc_b->kind == EntityKind::TimeEntity);
    CHECK(desc_b->data_key == "cam");
    CHECK(desc_b->time_value == 7);
}

TEST_CASE("writeTimePredictionsToGroups: idempotent entity registration", "[PredictionWriter]") {
    // Running predictions twice shouldn't create duplicate entities
    auto dm = makeDM("cam", 10);

    MLCore::PredictionOutput output;
    output.class_predictions = {0, 1};
    output.prediction_times = makeRowTimes({3, 4});

    MLCore::PredictionWriterConfig config;
    config.time_key_str = "cam";

    // Write twice
    auto groups1 = MLCore::writeTimePredictionsToGroups(
        *dm, output, {"A", "B"}, config);
    auto groups2 = MLCore::writeTimePredictionsToGroups(
        *dm, output, {"A", "B"}, config);

    // Different group IDs each time (new groups are created)
    CHECK(groups1[0] != groups2[0]);
    CHECK(groups1[1] != groups2[1]);

    // But entity IDs should be the same (ensureId is idempotent)
    auto * gm = dm->getEntityGroupManager();
    auto ents1 = gm->getEntitiesInGroup(groups1[0]);
    auto ents2 = gm->getEntitiesInGroup(groups2[0]);
    REQUIRE(ents1.size() == 1);
    REQUIRE(ents2.size() == 1);
    CHECK(ents1[0] == ents2[0]);
}

// ============================================================================
// writeClusterAssignmentsToGroups
// ============================================================================

TEST_CASE("writeClusterAssignmentsToGroups: basic 3 clusters", "[PredictionWriter]") {
    EntityGroupManager groups;
    EntityRegistry registry;

    // 6 entities in 3 clusters
    std::vector<EntityId> entity_ids;
    for (std::size_t i = 1; i <= 6; ++i) {
        entity_ids.push_back(EntityId{i});
    }

    arma::Row<std::size_t> assignments = {0, 1, 2, 0, 1, 2};

    auto group_ids = MLCore::writeClusterAssignmentsToGroups(
        groups, assignments,
        std::span<EntityId const>(entity_ids),
        "Cluster:");

    REQUIRE(group_ids.size() == 3);

    // Check group names
    for (std::size_t k = 0; k < 3; ++k) {
        auto desc = groups.getGroupDescriptor(group_ids[k]);
        REQUIRE(desc.has_value());
        CHECK(desc->name == "Cluster:" + std::to_string(k));
    }

    // Cluster 0: entities 1, 4
    CHECK(groups.getGroupSize(group_ids[0]) == 2);
    CHECK(groups.isEntityInGroup(group_ids[0], EntityId{1}));
    CHECK(groups.isEntityInGroup(group_ids[0], EntityId{4}));

    // Cluster 1: entities 2, 5
    CHECK(groups.getGroupSize(group_ids[1]) == 2);
    CHECK(groups.isEntityInGroup(group_ids[1], EntityId{2}));
    CHECK(groups.isEntityInGroup(group_ids[1], EntityId{5}));

    // Cluster 2: entities 3, 6
    CHECK(groups.getGroupSize(group_ids[2]) == 2);
    CHECK(groups.isEntityInGroup(group_ids[2], EntityId{3}));
    CHECK(groups.isEntityInGroup(group_ids[2], EntityId{6}));
}

TEST_CASE("writeClusterAssignmentsToGroups: empty input", "[PredictionWriter]") {
    EntityGroupManager groups;
    arma::Row<std::size_t> assignments;
    std::span<EntityId const> empty_ids;

    auto group_ids = MLCore::writeClusterAssignmentsToGroups(
        groups, assignments, empty_ids);

    CHECK(group_ids.empty());
}

TEST_CASE("writeClusterAssignmentsToGroups: size mismatch throws", "[PredictionWriter]") {
    EntityGroupManager groups;
    arma::Row<std::size_t> assignments = {0, 1, 0};
    std::vector<EntityId> entities = {EntityId{1}, EntityId{2}};

    CHECK_THROWS_AS(
        MLCore::writeClusterAssignmentsToGroups(
            groups, assignments,
            std::span<EntityId const>(entities)),
        std::invalid_argument);
}

TEST_CASE("writeClusterAssignmentsToGroups: custom prefix", "[PredictionWriter]") {
    EntityGroupManager groups;
    std::vector<EntityId> entities = {EntityId{10}, EntityId{20}};
    arma::Row<std::size_t> assignments = {0, 1};

    auto group_ids = MLCore::writeClusterAssignmentsToGroups(
        groups, assignments,
        std::span<EntityId const>(entities),
        "KMeans:");

    REQUIRE(group_ids.size() == 2);
    auto desc0 = groups.getGroupDescriptor(group_ids[0]);
    REQUIRE(desc0.has_value());
    CHECK(desc0->name == "KMeans:0");
}

// ============================================================================
// writePredictions (unified entry point)
// ============================================================================

TEST_CASE("writePredictions: all outputs enabled", "[PredictionWriter]") {
    auto dm = makeDM("cam", 20);

    MLCore::PredictionOutput output;
    output.class_predictions = {0, 0, 1, 1, 0, 1};
    output.prediction_times = makeRowTimes({0, 1, 2, 3, 4, 5});

    arma::mat probs(2, 6);
    probs.row(0) = arma::rowvec({0.8, 0.7, 0.3, 0.2, 0.6, 0.4});
    probs.row(1) = arma::rowvec({0.2, 0.3, 0.7, 0.8, 0.4, 0.6});
    output.class_probabilities = probs;

    MLCore::PredictionWriterConfig config;
    config.time_key_str = "cam";
    config.output_prefix = "ML:";
    config.write_intervals = true;
    config.write_probabilities = true;
    config.write_to_putative_groups = true;

    auto result = MLCore::writePredictions(*dm, output, {"No", "Yes"}, config);

    // Check result structure
    CHECK(result.class_names.size() == 2);
    CHECK(result.interval_keys.size() == 2);
    CHECK(result.probability_keys.size() == 2);
    CHECK(result.putative_group_ids.size() == 2);

    // Verify interval keys exist in DataManager
    for (auto const & key : result.interval_keys) {
        CHECK(dm->getData<DigitalIntervalSeries>(key) != nullptr);
    }

    // Verify probability keys exist in DataManager
    for (auto const & key : result.probability_keys) {
        CHECK(dm->getData<AnalogTimeSeries>(key) != nullptr);
    }

    // Verify groups exist
    auto * gm = dm->getEntityGroupManager();
    for (auto gid : result.putative_group_ids) {
        CHECK(gm->hasGroup(gid));
    }
}

TEST_CASE("writePredictions: only intervals enabled", "[PredictionWriter]") {
    auto dm = makeDM("cam", 20);
    auto output = makeBinaryPrediction({0, 1, 0});

    MLCore::PredictionWriterConfig config;
    config.time_key_str = "cam";
    config.write_intervals = true;
    config.write_probabilities = false;
    config.write_to_putative_groups = false;

    auto result = MLCore::writePredictions(*dm, output, {"A", "B"}, config);

    CHECK(result.interval_keys.size() == 2);
    CHECK(result.probability_keys.empty());
    CHECK(result.putative_group_ids.empty());
}

TEST_CASE("writePredictions: everything disabled produces empty result", "[PredictionWriter]") {
    auto dm = makeDM("cam", 10);
    auto output = makeBinaryPrediction({0, 1});

    MLCore::PredictionWriterConfig config;
    config.time_key_str = "cam";
    config.write_intervals = false;
    config.write_probabilities = false;
    config.write_to_putative_groups = false;

    auto result = MLCore::writePredictions(*dm, output, {"A", "B"}, config);

    CHECK(result.interval_keys.empty());
    CHECK(result.probability_keys.empty());
    CHECK(result.putative_group_ids.empty());
    CHECK(result.class_names.size() == 2); // class_names always set
}

// ============================================================================
// Validation / error handling
// ============================================================================

TEST_CASE("writePredictions: prediction/time size mismatch throws", "[PredictionWriter]") {
    auto dm = makeDM("cam", 10);

    MLCore::PredictionOutput output;
    output.class_predictions = {0, 1, 0}; // 3 predictions
    output.prediction_times = makeRowTimes({0, 1}); // 2 times — mismatch

    CHECK_THROWS_AS(
        MLCore::writePredictions(*dm, output, {"A", "B"}),
        std::invalid_argument);
}

TEST_CASE("writePredictions: empty prediction_times throws", "[PredictionWriter]") {
    auto dm = makeDM("cam", 10);

    MLCore::PredictionOutput output;
    // Both empty to maintain size match, but still invalid
    output.class_predictions = arma::Row<std::size_t>();
    output.prediction_times = {};

    CHECK_THROWS_AS(
        MLCore::writePredictions(*dm, output, {"A"}),
        std::invalid_argument);
}

TEST_CASE("writePredictions: empty class_names throws", "[PredictionWriter]") {
    auto dm = makeDM("cam", 10);
    auto output = makeBinaryPrediction({0, 0});

    CHECK_THROWS_AS(
        MLCore::writePredictions(*dm, output, {}),
        std::invalid_argument);
}

TEST_CASE("writePredictions: label exceeds class_names range throws", "[PredictionWriter]") {
    auto dm = makeDM("cam", 10);

    MLCore::PredictionOutput output;
    output.class_predictions = {0, 2}; // label 2, but only 2 class_names
    output.prediction_times = makeRowTimes({0, 1});

    CHECK_THROWS_AS(
        MLCore::writePredictions(*dm, output, {"A", "B"}),
        std::invalid_argument);
}

TEST_CASE("writeProbabilitiesAsAnalog: dimension mismatch throws", "[PredictionWriter]") {
    auto dm = makeDM("cam", 10);

    MLCore::PredictionOutput output;
    output.class_predictions = {0, 1};
    output.prediction_times = makeRowTimes({0, 1});

    // Wrong number of columns (3 instead of 2)
    arma::mat bad_probs(2, 3); bad_probs.fill(0.5);
    output.class_probabilities = bad_probs;

    CHECK_THROWS_AS(
        MLCore::writeProbabilitiesAsAnalog(*dm, output, {"A", "B"}),
        std::invalid_argument);
}
