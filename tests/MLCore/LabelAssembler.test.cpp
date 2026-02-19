
#include <catch2/catch_test_macros.hpp>

#include "features/LabelAssembler.hpp"

#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityRegistry.hpp"
#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <memory>
#include <string>
#include <vector>

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
 * @brief Create a DigitalIntervalSeries with given interval bounds
 *
 * Each pair in intervals_vec is (start, end) inclusive.
 * The series is assigned the given TimeFrame.
 */
std::shared_ptr<DigitalIntervalSeries> makeIntervalSeries(
    std::vector<std::pair<int64_t, int64_t>> const & intervals_vec,
    std::shared_ptr<TimeFrame> tf)
{
    auto series = std::make_shared<DigitalIntervalSeries>();
    series->setTimeFrame(tf);
    for (auto const & [s, e] : intervals_vec) {
        series->addEvent(TimeFrameIndex(s), TimeFrameIndex(e));
    }
    return series;
}

/**
 * @brief Build a vector of TimeFrameIndex from a list of int64_t values
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
 * @brief Register TimeEntity IDs for a set of time values and add them to a group
 *
 * @return Vector of registered EntityIds
 */
std::vector<EntityId> registerTimeEntitiesInGroup(
    EntityRegistry & registry,
    EntityGroupManager & groups,
    GroupId group_id,
    std::string const & time_key,
    std::vector<int64_t> const & time_values)
{
    std::vector<EntityId> ids;
    ids.reserve(time_values.size());
    for (auto tv : time_values) {
        auto eid = registry.ensureId(time_key, EntityKind::TimeEntity, TimeFrameIndex(tv), 0);
        ids.push_back(eid);
        groups.addEntityToGroup(group_id, eid);
    }
    return ids;
}

/**
 * @brief Register data entities and add them to a group
 */
std::vector<EntityId> registerDataEntitiesInGroup(
    EntityRegistry & registry,
    EntityGroupManager & groups,
    GroupId group_id,
    std::string const & data_key,
    EntityKind kind,
    std::vector<int64_t> const & time_values)
{
    std::vector<EntityId> ids;
    ids.reserve(time_values.size());
    for (auto tv : time_values) {
        auto eid = registry.ensureId(data_key, kind, TimeFrameIndex(tv), 0);
        ids.push_back(eid);
        groups.addEntityToGroup(group_id, eid);
    }
    return ids;
}

} // anonymous namespace

// ============================================================================
// assembleLabelsFromIntervals — binary labeling
// ============================================================================

TEST_CASE("LabelAssembler: binary labels from intervals basic", "[LabelAssembler]") {
    auto tf = makeTimeFrame(100);
    // Intervals: [10, 20] and [30, 40]
    auto intervals = makeIntervalSeries({{10, 20}, {30, 40}}, tf);

    auto row_times = makeRowTimes({5, 10, 15, 20, 25, 30, 35, 40, 45});

    auto result = MLCore::assembleLabelsFromIntervals(*intervals, *tf, row_times);

    REQUIRE(result.num_classes == 2);
    REQUIRE(result.labels.n_elem == 9);
    CHECK(result.unlabeled_count == 0);

    // Expected: outside, inside×3, outside, inside×3, outside
    CHECK(result.labels(0) == 0);  // 5  → outside
    CHECK(result.labels(1) == 1);  // 10 → inside (interval start)
    CHECK(result.labels(2) == 1);  // 15 → inside
    CHECK(result.labels(3) == 1);  // 20 → inside (interval end)
    CHECK(result.labels(4) == 0);  // 25 → outside (gap)
    CHECK(result.labels(5) == 1);  // 30 → inside (second interval start)
    CHECK(result.labels(6) == 1);  // 35 → inside
    CHECK(result.labels(7) == 1);  // 40 → inside (second interval end)
    CHECK(result.labels(8) == 0);  // 45 → outside
}

TEST_CASE("LabelAssembler: binary labels class names default", "[LabelAssembler]") {
    auto tf = makeTimeFrame(50);
    auto intervals = makeIntervalSeries({{10, 20}}, tf);
    auto row_times = makeRowTimes({5, 15});

    auto result = MLCore::assembleLabelsFromIntervals(*intervals, *tf, row_times);

    REQUIRE(result.class_names.size() == 2);
    CHECK(result.class_names[0] == "Outside");
    CHECK(result.class_names[1] == "Inside");
}

TEST_CASE("LabelAssembler: binary labels custom class names", "[LabelAssembler]") {
    auto tf = makeTimeFrame(50);
    auto intervals = makeIntervalSeries({{10, 20}}, tf);
    auto row_times = makeRowTimes({5, 15});

    MLCore::LabelFromIntervals config;
    config.positive_class_name = "Contact";
    config.negative_class_name = "No Contact";

    auto result = MLCore::assembleLabelsFromIntervals(*intervals, *tf, row_times, config);

    REQUIRE(result.class_names.size() == 2);
    CHECK(result.class_names[0] == "No Contact");
    CHECK(result.class_names[1] == "Contact");
}

TEST_CASE("LabelAssembler: binary all inside", "[LabelAssembler]") {
    auto tf = makeTimeFrame(100);
    auto intervals = makeIntervalSeries({{0, 99}}, tf);
    auto row_times = makeRowTimes({0, 25, 50, 75, 99});

    auto result = MLCore::assembleLabelsFromIntervals(*intervals, *tf, row_times);

    for (arma::uword i = 0; i < result.labels.n_elem; ++i) {
        CHECK(result.labels(i) == 1);
    }
}

TEST_CASE("LabelAssembler: binary all outside", "[LabelAssembler]") {
    auto tf = makeTimeFrame(100);
    // Empty interval series
    auto intervals = makeIntervalSeries({}, tf);
    auto row_times = makeRowTimes({0, 25, 50, 75, 99});

    auto result = MLCore::assembleLabelsFromIntervals(*intervals, *tf, row_times);

    for (arma::uword i = 0; i < result.labels.n_elem; ++i) {
        CHECK(result.labels(i) == 0);
    }
}

TEST_CASE("LabelAssembler: binary single row_time", "[LabelAssembler]") {
    auto tf = makeTimeFrame(50);
    auto intervals = makeIntervalSeries({{10, 20}}, tf);
    auto row_times = makeRowTimes({15});

    auto result = MLCore::assembleLabelsFromIntervals(*intervals, *tf, row_times);

    REQUIRE(result.labels.n_elem == 1);
    CHECK(result.labels(0) == 1);
}

TEST_CASE("LabelAssembler: binary throws on empty row_times", "[LabelAssembler]") {
    auto tf = makeTimeFrame(50);
    auto intervals = makeIntervalSeries({{10, 20}}, tf);
    std::vector<TimeFrameIndex> empty_times;

    CHECK_THROWS_AS(
        MLCore::assembleLabelsFromIntervals(*intervals, *tf, empty_times),
        std::invalid_argument);
}

// ============================================================================
// assembleLabelsFromTimeEntityGroups — multi-class from TimeEntity groups
// ============================================================================

TEST_CASE("LabelAssembler: time entity groups binary", "[LabelAssembler]") {
    EntityRegistry registry;
    EntityGroupManager groups;

    std::string const time_key = "camera";

    // Create two groups: "Contact" (class 0) and "No Contact" (class 1)
    auto g_contact = groups.createGroup("Contact");
    auto g_no_contact = groups.createGroup("No Contact");

    // Register time entities and assign to groups
    // Contact at frames 10, 15, 20
    registerTimeEntitiesInGroup(registry, groups, g_contact, time_key, {10, 15, 20});
    // No Contact at frames 5, 25, 30
    registerTimeEntitiesInGroup(registry, groups, g_no_contact, time_key, {5, 25, 30});

    auto row_times = makeRowTimes({5, 10, 15, 20, 25, 30});

    MLCore::LabelFromTimeEntityGroups config;
    config.class_groups = {g_contact, g_no_contact};
    config.time_key = time_key;

    auto result = MLCore::assembleLabelsFromTimeEntityGroups(groups, registry, row_times, config);

    REQUIRE(result.num_classes == 2);
    REQUIRE(result.labels.n_elem == 6);
    CHECK(result.unlabeled_count == 0);

    CHECK(result.labels(0) == 1);  // frame 5  → No Contact (class 1)
    CHECK(result.labels(1) == 0);  // frame 10 → Contact (class 0)
    CHECK(result.labels(2) == 0);  // frame 15 → Contact (class 0)
    CHECK(result.labels(3) == 0);  // frame 20 → Contact (class 0)
    CHECK(result.labels(4) == 1);  // frame 25 → No Contact (class 1)
    CHECK(result.labels(5) == 1);  // frame 30 → No Contact (class 1)

    // Check class names come from group names
    REQUIRE(result.class_names.size() == 2);
    CHECK(result.class_names[0] == "Contact");
    CHECK(result.class_names[1] == "No Contact");
}

TEST_CASE("LabelAssembler: time entity groups with unlabeled rows", "[LabelAssembler]") {
    EntityRegistry registry;
    EntityGroupManager groups;

    std::string const time_key = "camera";
    auto g_class0 = groups.createGroup("ClassA");

    // Only frames 10 and 20 are labeled
    registerTimeEntitiesInGroup(registry, groups, g_class0, time_key, {10, 20});

    // Row times include unlabeled frames
    auto row_times = makeRowTimes({5, 10, 15, 20, 25});

    MLCore::LabelFromTimeEntityGroups config;
    config.class_groups = {g_class0};
    config.time_key = time_key;

    auto result = MLCore::assembleLabelsFromTimeEntityGroups(groups, registry, row_times, config);

    REQUIRE(result.num_classes == 1);
    CHECK(result.unlabeled_count == 3);  // frames 5, 15, 25 unlabeled

    CHECK(result.labels(0) == 1);  // frame 5  → unlabeled (sentinel = num_classes = 1)
    CHECK(result.labels(1) == 0);  // frame 10 → ClassA (class 0)
    CHECK(result.labels(2) == 1);  // frame 15 → unlabeled
    CHECK(result.labels(3) == 0);  // frame 20 → ClassA (class 0)
    CHECK(result.labels(4) == 1);  // frame 25 → unlabeled
}

TEST_CASE("LabelAssembler: time entity groups three classes", "[LabelAssembler]") {
    EntityRegistry registry;
    EntityGroupManager groups;

    std::string const time_key = "camera";
    auto g0 = groups.createGroup("Rest");
    auto g1 = groups.createGroup("Walk");
    auto g2 = groups.createGroup("Run");

    registerTimeEntitiesInGroup(registry, groups, g0, time_key, {0, 1, 2});
    registerTimeEntitiesInGroup(registry, groups, g1, time_key, {10, 11, 12});
    registerTimeEntitiesInGroup(registry, groups, g2, time_key, {20, 21, 22});

    auto row_times = makeRowTimes({0, 1, 2, 10, 11, 12, 20, 21, 22});

    MLCore::LabelFromTimeEntityGroups config;
    config.class_groups = {g0, g1, g2};
    config.time_key = time_key;

    auto result = MLCore::assembleLabelsFromTimeEntityGroups(groups, registry, row_times, config);

    REQUIRE(result.num_classes == 3);
    REQUIRE(result.labels.n_elem == 9);
    CHECK(result.unlabeled_count == 0);

    // Rest = 0, Walk = 1, Run = 2
    CHECK(result.labels(0) == 0);
    CHECK(result.labels(1) == 0);
    CHECK(result.labels(2) == 0);
    CHECK(result.labels(3) == 1);
    CHECK(result.labels(4) == 1);
    CHECK(result.labels(5) == 1);
    CHECK(result.labels(6) == 2);
    CHECK(result.labels(7) == 2);
    CHECK(result.labels(8) == 2);

    REQUIRE(result.class_names.size() == 3);
    CHECK(result.class_names[0] == "Rest");
    CHECK(result.class_names[1] == "Walk");
    CHECK(result.class_names[2] == "Run");
}

TEST_CASE("LabelAssembler: time entity groups ignores different time_key", "[LabelAssembler]") {
    EntityRegistry registry;
    EntityGroupManager groups;

    auto g0 = groups.createGroup("ClassA");

    // Register entities with different time keys
    auto eid_camera = registry.ensureId("camera", EntityKind::TimeEntity, TimeFrameIndex(10), 0);
    auto eid_ephys = registry.ensureId("ephys", EntityKind::TimeEntity, TimeFrameIndex(10), 0);
    groups.addEntityToGroup(g0, eid_camera);
    groups.addEntityToGroup(g0, eid_ephys);

    auto row_times = makeRowTimes({10});

    // Only match "camera" entities
    MLCore::LabelFromTimeEntityGroups config;
    config.class_groups = {g0};
    config.time_key = "camera";

    auto result = MLCore::assembleLabelsFromTimeEntityGroups(groups, registry, row_times, config);

    // Should find the match via the camera entity
    CHECK(result.labels(0) == 0);
    CHECK(result.unlabeled_count == 0);
}

TEST_CASE("LabelAssembler: time entity groups ignores non-TimeEntity kinds", "[LabelAssembler]") {
    EntityRegistry registry;
    EntityGroupManager groups;

    auto g0 = groups.createGroup("ClassA");

    // Register a TimeEntity and a LineEntity at the same time_key/time_value
    auto time_eid = registry.ensureId("camera", EntityKind::TimeEntity, TimeFrameIndex(10), 0);
    auto line_eid = registry.ensureId("camera", EntityKind::LineEntity, TimeFrameIndex(10), 0);
    groups.addEntityToGroup(g0, time_eid);
    groups.addEntityToGroup(g0, line_eid);

    auto row_times = makeRowTimes({10, 20});

    MLCore::LabelFromTimeEntityGroups config;
    config.class_groups = {g0};
    config.time_key = "camera";

    auto result = MLCore::assembleLabelsFromTimeEntityGroups(groups, registry, row_times, config);

    // frame 10 matched via TimeEntity, frame 20 unlabeled
    CHECK(result.labels(0) == 0);
    CHECK(result.labels(1) == 1);  // sentinel = num_classes
    CHECK(result.unlabeled_count == 1);
}

TEST_CASE("LabelAssembler: time entity groups first group wins on collision", "[LabelAssembler]") {
    EntityRegistry registry;
    EntityGroupManager groups;

    std::string const time_key = "camera";
    auto g0 = groups.createGroup("GroupA");
    auto g1 = groups.createGroup("GroupB");

    // Frame 10 is in both groups — g0 (label 0) should win
    registerTimeEntitiesInGroup(registry, groups, g0, time_key, {10});
    // Same entity in g1 — the entity already exists, so ensureId returns the same ID
    auto eid = registry.ensureId(time_key, EntityKind::TimeEntity, TimeFrameIndex(10), 0);
    groups.addEntityToGroup(g1, eid);

    auto row_times = makeRowTimes({10});

    MLCore::LabelFromTimeEntityGroups config;
    config.class_groups = {g0, g1};
    config.time_key = time_key;

    auto result = MLCore::assembleLabelsFromTimeEntityGroups(groups, registry, row_times, config);

    // First group (g0) should win → label 0
    CHECK(result.labels(0) == 0);
}

TEST_CASE("LabelAssembler: time entity groups throws on empty class_groups", "[LabelAssembler]") {
    EntityRegistry registry;
    EntityGroupManager groups;

    auto row_times = makeRowTimes({10});

    MLCore::LabelFromTimeEntityGroups config;
    config.class_groups = {};
    config.time_key = "camera";

    CHECK_THROWS_AS(
        MLCore::assembleLabelsFromTimeEntityGroups(groups, registry, row_times, config),
        std::invalid_argument);
}

TEST_CASE("LabelAssembler: time entity groups throws on empty row_times", "[LabelAssembler]") {
    EntityRegistry registry;
    EntityGroupManager groups;

    auto g0 = groups.createGroup("A");
    std::vector<TimeFrameIndex> empty_times;

    MLCore::LabelFromTimeEntityGroups config;
    config.class_groups = {g0};
    config.time_key = "camera";

    CHECK_THROWS_AS(
        MLCore::assembleLabelsFromTimeEntityGroups(groups, registry, empty_times, config),
        std::invalid_argument);
}

// ============================================================================
// assembleLabelsFromDataEntityGroups — multi-class from data entity groups
// ============================================================================

TEST_CASE("LabelAssembler: data entity groups basic", "[LabelAssembler]") {
    EntityRegistry registry;
    EntityGroupManager groups;

    std::string const data_key = "whisker0";
    auto g0 = groups.createGroup("Protraction");
    auto g1 = groups.createGroup("Retraction");

    // Register LineEntity entities at specific times
    registerDataEntitiesInGroup(registry, groups, g0, data_key, EntityKind::LineEntity, {10, 20, 30});
    registerDataEntitiesInGroup(registry, groups, g1, data_key, EntityKind::LineEntity, {40, 50, 60});

    auto row_times = makeRowTimes({10, 20, 30, 40, 50, 60});

    MLCore::LabelFromDataEntityGroups config;
    config.data_key = data_key;
    config.class_groups = {g0, g1};

    auto result = MLCore::assembleLabelsFromDataEntityGroups(groups, registry, row_times, config);

    REQUIRE(result.num_classes == 2);
    REQUIRE(result.labels.n_elem == 6);
    CHECK(result.unlabeled_count == 0);

    CHECK(result.labels(0) == 0);  // 10 → Protraction
    CHECK(result.labels(1) == 0);  // 20 → Protraction
    CHECK(result.labels(2) == 0);  // 30 → Protraction
    CHECK(result.labels(3) == 1);  // 40 → Retraction
    CHECK(result.labels(4) == 1);  // 50 → Retraction
    CHECK(result.labels(5) == 1);  // 60 → Retraction

    REQUIRE(result.class_names.size() == 2);
    CHECK(result.class_names[0] == "Protraction");
    CHECK(result.class_names[1] == "Retraction");
}

TEST_CASE("LabelAssembler: data entity groups ignores different data_key", "[LabelAssembler]") {
    EntityRegistry registry;
    EntityGroupManager groups;

    auto g0 = groups.createGroup("ClassA");

    // Register entities with different data keys
    registerDataEntitiesInGroup(registry, groups, g0, "whisker0", EntityKind::LineEntity, {10});
    registerDataEntitiesInGroup(registry, groups, g0, "whisker1", EntityKind::LineEntity, {10});

    auto row_times = makeRowTimes({10});

    // Only match "whisker0" entities
    MLCore::LabelFromDataEntityGroups config;
    config.data_key = "whisker0";
    config.class_groups = {g0};

    auto result = MLCore::assembleLabelsFromDataEntityGroups(groups, registry, row_times, config);

    CHECK(result.labels(0) == 0);
    CHECK(result.unlabeled_count == 0);
}

TEST_CASE("LabelAssembler: data entity groups with unlabeled rows", "[LabelAssembler]") {
    EntityRegistry registry;
    EntityGroupManager groups;

    auto g0 = groups.createGroup("Labeled");
    registerDataEntitiesInGroup(registry, groups, g0, "whisker0", EntityKind::LineEntity, {10, 20});

    auto row_times = makeRowTimes({5, 10, 15, 20, 25});

    MLCore::LabelFromDataEntityGroups config;
    config.data_key = "whisker0";
    config.class_groups = {g0};

    auto result = MLCore::assembleLabelsFromDataEntityGroups(groups, registry, row_times, config);

    CHECK(result.unlabeled_count == 3);  // frames 5, 15, 25
    CHECK(result.labels(0) == 1);  // sentinel = num_classes = 1
    CHECK(result.labels(1) == 0);  // labeled
    CHECK(result.labels(2) == 1);  // sentinel
    CHECK(result.labels(3) == 0);  // labeled
    CHECK(result.labels(4) == 1);  // sentinel
}

TEST_CASE("LabelAssembler: data entity groups accepts mixed entity kinds", "[LabelAssembler]") {
    EntityRegistry registry;
    EntityGroupManager groups;

    auto g0 = groups.createGroup("Mixed");

    // Register entities of different kinds but same data_key
    auto eid1 = registry.ensureId("mydata", EntityKind::LineEntity, TimeFrameIndex(10), 0);
    auto eid2 = registry.ensureId("mydata", EntityKind::PointEntity, TimeFrameIndex(20), 0);
    auto eid3 = registry.ensureId("mydata", EntityKind::EventEntity, TimeFrameIndex(30), 0);
    groups.addEntityToGroup(g0, eid1);
    groups.addEntityToGroup(g0, eid2);
    groups.addEntityToGroup(g0, eid3);

    auto row_times = makeRowTimes({10, 20, 30});

    MLCore::LabelFromDataEntityGroups config;
    config.data_key = "mydata";
    config.class_groups = {g0};

    auto result = MLCore::assembleLabelsFromDataEntityGroups(groups, registry, row_times, config);

    // All should be labeled as class 0 (no kind filter)
    CHECK(result.labels(0) == 0);
    CHECK(result.labels(1) == 0);
    CHECK(result.labels(2) == 0);
    CHECK(result.unlabeled_count == 0);
}

TEST_CASE("LabelAssembler: data entity groups throws on empty class_groups", "[LabelAssembler]") {
    EntityRegistry registry;
    EntityGroupManager groups;

    auto row_times = makeRowTimes({10});

    MLCore::LabelFromDataEntityGroups config;
    config.data_key = "whisker0";
    config.class_groups = {};

    CHECK_THROWS_AS(
        MLCore::assembleLabelsFromDataEntityGroups(groups, registry, row_times, config),
        std::invalid_argument);
}

// ============================================================================
// getClassNamesFromGroups
// ============================================================================

TEST_CASE("LabelAssembler: getClassNamesFromGroups returns group names", "[LabelAssembler]") {
    EntityGroupManager groups;

    auto g1 = groups.createGroup("Contact");
    auto g2 = groups.createGroup("No Contact");
    auto g3 = groups.createGroup("Ambiguous");

    auto names = MLCore::getClassNamesFromGroups(groups, {g1, g2, g3});

    REQUIRE(names.size() == 3);
    CHECK(names[0] == "Contact");
    CHECK(names[1] == "No Contact");
    CHECK(names[2] == "Ambiguous");
}

TEST_CASE("LabelAssembler: getClassNamesFromGroups fallback for missing groups", "[LabelAssembler]") {
    EntityGroupManager groups;

    auto g1 = groups.createGroup("Existing");
    GroupId nonexistent_id = 9999;

    auto names = MLCore::getClassNamesFromGroups(groups, {g1, nonexistent_id});

    REQUIRE(names.size() == 2);
    CHECK(names[0] == "Existing");
    CHECK(names[1] == "Class_9999");
}

TEST_CASE("LabelAssembler: getClassNamesFromGroups empty input", "[LabelAssembler]") {
    EntityGroupManager groups;

    auto names = MLCore::getClassNamesFromGroups(groups, {});

    CHECK(names.empty());
}

// ============================================================================
// countRowsInsideIntervals
// ============================================================================

TEST_CASE("LabelAssembler: countRowsInsideIntervals basic", "[LabelAssembler]") {
    auto tf = makeTimeFrame(100);
    auto intervals = makeIntervalSeries({{10, 20}, {30, 40}}, tf);

    auto row_times = makeRowTimes({5, 10, 15, 25, 35, 45});

    auto count = MLCore::countRowsInsideIntervals(*intervals, *tf, row_times);

    // Inside: 10, 15, 35 → count = 3
    CHECK(count == 3);
}

TEST_CASE("LabelAssembler: countRowsInsideIntervals all inside", "[LabelAssembler]") {
    auto tf = makeTimeFrame(100);
    auto intervals = makeIntervalSeries({{0, 99}}, tf);

    auto row_times = makeRowTimes({0, 50, 99});

    CHECK(MLCore::countRowsInsideIntervals(*intervals, *tf, row_times) == 3);
}

TEST_CASE("LabelAssembler: countRowsInsideIntervals none inside", "[LabelAssembler]") {
    auto tf = makeTimeFrame(100);
    auto intervals = makeIntervalSeries({}, tf);

    auto row_times = makeRowTimes({0, 50, 99});

    CHECK(MLCore::countRowsInsideIntervals(*intervals, *tf, row_times) == 0);
}

TEST_CASE("LabelAssembler: countRowsInsideIntervals empty row_times", "[LabelAssembler]") {
    auto tf = makeTimeFrame(100);
    auto intervals = makeIntervalSeries({{10, 20}}, tf);

    std::vector<TimeFrameIndex> empty_times;

    CHECK(MLCore::countRowsInsideIntervals(*intervals, *tf, empty_times) == 0);
}

// ============================================================================
// Edge cases
// ============================================================================

TEST_CASE("LabelAssembler: interval boundary inclusivity", "[LabelAssembler]") {
    auto tf = makeTimeFrame(100);
    // Single interval [10, 20]
    auto intervals = makeIntervalSeries({{10, 20}}, tf);

    // Test exact boundaries
    auto row_times = makeRowTimes({9, 10, 20, 21});

    auto result = MLCore::assembleLabelsFromIntervals(*intervals, *tf, row_times);

    CHECK(result.labels(0) == 0);  // 9  → outside (just before)
    CHECK(result.labels(1) == 1);  // 10 → inside (start boundary)
    CHECK(result.labels(2) == 1);  // 20 → inside (end boundary)
    CHECK(result.labels(3) == 0);  // 21 → outside (just after)
}

TEST_CASE("LabelAssembler: adjacent intervals", "[LabelAssembler]") {
    auto tf = makeTimeFrame(100);
    // Adjacent intervals: [10, 19] and [20, 29]
    auto intervals = makeIntervalSeries({{10, 19}, {20, 29}}, tf);

    auto row_times = makeRowTimes({9, 10, 19, 20, 29, 30});

    auto result = MLCore::assembleLabelsFromIntervals(*intervals, *tf, row_times);

    CHECK(result.labels(0) == 0);  // 9  → outside
    CHECK(result.labels(1) == 1);  // 10 → inside first
    CHECK(result.labels(2) == 1);  // 19 → inside first (end)
    CHECK(result.labels(3) == 1);  // 20 → inside second (start)
    CHECK(result.labels(4) == 1);  // 29 → inside second (end)
    CHECK(result.labels(5) == 0);  // 30 → outside
}

TEST_CASE("LabelAssembler: time entity groups with empty groups", "[LabelAssembler]") {
    EntityRegistry registry;
    EntityGroupManager groups;

    auto g_empty = groups.createGroup("Empty");
    auto g_filled = groups.createGroup("Filled");

    registerTimeEntitiesInGroup(registry, groups, g_filled, "camera", {10, 20});

    auto row_times = makeRowTimes({10, 20});

    MLCore::LabelFromTimeEntityGroups config;
    config.class_groups = {g_empty, g_filled};
    config.time_key = "camera";

    auto result = MLCore::assembleLabelsFromTimeEntityGroups(groups, registry, row_times, config);

    // All rows should be labeled as class 1 (g_filled) since g_empty has no entities
    CHECK(result.labels(0) == 1);
    CHECK(result.labels(1) == 1);
    CHECK(result.unlabeled_count == 0);
}
