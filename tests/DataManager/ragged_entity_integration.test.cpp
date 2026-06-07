/**
 * @file ragged_entity_integration.test.cpp
 * @brief Integration tests for copy/move-by-EntityID on RaggedTimeSeries types
 *
 * These tests require DataManager (heavy dependency) to wire up the
 * EntityRegistry. They verify that copyByEntityIds / moveByEntityIds work
 * correctly for every RaggedTimeSeries-derived type.
 *
 * To add a new type, add a RaggedTestTraits specialization and append the
 * type to the TEMPLATE_TEST_CASE type lists below.
 */

#include "DataManager.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "fixtures/RaggedTestTraits.hpp"
#include "fixtures/entity_id.hpp"

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <unordered_set>
#include <vector>

// ============================================================================
// Helper: create a DataManager with a standard timeframe, register source
// and target data objects of type T, and return a tuple for concise access.
// ============================================================================
template<typename T>
struct EntityTestFixture {
    std::unique_ptr<DataManager> dm;
    std::shared_ptr<T> source;
    std::shared_ptr<T> target;

    EntityTestFixture() {
        dm = std::make_unique<DataManager>();
        auto tf = std::make_shared<TimeFrame>(std::vector<int>{0, 10, 20, 30});
        dm->setTime(TimeKey("test_time"), tf);
        dm->template setData<T>("source_data", TimeKey("test_time"));
        dm->template setData<T>("target_data", TimeKey("test_time"));
        source = dm->template getData<T>("source_data");
        target = dm->template getData<T>("target_data");
    }
};

// ============================================================================
// Copy by EntityID
// ============================================================================
TEMPLATE_TEST_CASE("RaggedTimeSeries+DM - Copy by EntityID",
                   "[ragged][entity][copy][integration]",
                   LineData, MaskData, PointData) {

    using Traits = RaggedTestTraits<TestType>;
    EntityTestFixture<TestType> const fx;

    // Populate source: 2 items at t=10, 1 at t=20
    Traits::add(*fx.source, TimeFrameIndex(10), Traits::sample1());
    Traits::add(*fx.source, TimeFrameIndex(10), Traits::sample2());
    Traits::add(*fx.source, TimeFrameIndex(20), Traits::sample3());

    SECTION("Basic copy — single time") {
        auto ids_10_view = fx.source->getEntityIdsAtTime(TimeFrameIndex(10));
        std::vector<EntityId> ids_10(ids_10_view.begin(), ids_10_view.end());
        REQUIRE(ids_10.size() == 2);

        std::unordered_set<EntityId> const id_set(ids_10.begin(), ids_10.end());
        std::size_t const copied = fx.source->copyByEntityIds(*fx.target, id_set, NotifyObservers::No);

        REQUIRE(copied == 2);

        // Source unchanged
        REQUIRE(fx.source->getAtTime(TimeFrameIndex(10)).size() == 2);
        REQUIRE(fx.source->getAtTime(TimeFrameIndex(20)).size() == 1);

        // Target has copies
        REQUIRE(fx.target->getAtTime(TimeFrameIndex(10)).size() == 2);
        REQUIRE(fx.target->getAtTime(TimeFrameIndex(20)).empty());

        // Target entity IDs differ from source
        auto target_ids = get_all_entity_ids(*fx.target);
        REQUIRE(target_ids.size() == 2);
        REQUIRE(target_ids != ids_10);
    }

    SECTION("Copy — mixed times") {
        auto ids_10 = fx.source->getEntityIdsAtTime(TimeFrameIndex(10));
        auto ids_20 = fx.source->getEntityIdsAtTime(TimeFrameIndex(20));

        std::vector<EntityId> mixed = {ids_10[0], ids_20[0]};
        std::unordered_set<EntityId> const id_set(mixed.begin(), mixed.end());
        std::size_t const copied = fx.source->copyByEntityIds(*fx.target, id_set, NotifyObservers::No);

        REQUIRE(copied == 2);
        REQUIRE(fx.target->getAtTime(TimeFrameIndex(10)).size() == 1);
        REQUIRE(fx.target->getAtTime(TimeFrameIndex(20)).size() == 1);
    }

    SECTION("Copy — non-existent EntityIDs") {
        std::unordered_set<EntityId> const fake = {EntityId(99999), EntityId(88888)};
        std::size_t const copied = fx.source->copyByEntityIds(*fx.target, fake, NotifyObservers::No);

        REQUIRE(copied == 0);
        REQUIRE(fx.target->getTimesWithData().empty());
    }

    SECTION("Copy — empty EntityID set") {
        std::unordered_set<EntityId> const empty_set;
        std::size_t const copied = fx.source->copyByEntityIds(*fx.target, empty_set, NotifyObservers::No);

        REQUIRE(copied == 0);
        REQUIRE(fx.target->getTimesWithData().empty());
    }
}

// ============================================================================
// Move by EntityID
// ============================================================================
TEMPLATE_TEST_CASE("RaggedTimeSeries+DM - Move by EntityID",
                   "[ragged][entity][move][integration]",
                   LineData, MaskData, PointData) {

    using Traits = RaggedTestTraits<TestType>;
    EntityTestFixture<TestType> const fx;

    // Populate source: 2 items at t=10, 1 at t=20
    Traits::add(*fx.source, TimeFrameIndex(10), Traits::sample1());
    Traits::add(*fx.source, TimeFrameIndex(10), Traits::sample2());
    Traits::add(*fx.source, TimeFrameIndex(20), Traits::sample3());

    SECTION("Basic move — single time") {
        auto ids_10_view = fx.source->getEntityIdsAtTime(TimeFrameIndex(10));
        std::vector<EntityId> ids_10(ids_10_view.begin(), ids_10_view.end());
        REQUIRE(ids_10.size() == 2);

        std::unordered_set<EntityId> const id_set(ids_10.begin(), ids_10.end());
        std::size_t const moved = fx.source->moveByEntityIds(*fx.target, id_set, NotifyObservers::No);

        REQUIRE(moved == 2);

        // Source lost t=10 entries
        REQUIRE(fx.source->getAtTime(TimeFrameIndex(10)).size() == 0);
        REQUIRE(fx.source->getAtTime(TimeFrameIndex(20)).size() == 1);

        // Target gained them
        REQUIRE(fx.target->getAtTime(TimeFrameIndex(10)).size() == 2);
        REQUIRE(fx.target->getAtTime(TimeFrameIndex(20)).empty());

        // Target preserves original entity IDs
        auto target_ids = get_all_entity_ids(*fx.target);
        REQUIRE(target_ids == ids_10);
    }

    SECTION("Move — mixed times") {
        auto ids_10_view = fx.source->getEntityIdsAtTime(TimeFrameIndex(10));
        auto ids_20_view = fx.source->getEntityIdsAtTime(TimeFrameIndex(20));
        std::vector<EntityId> ids_10(ids_10_view.begin(), ids_10_view.end());
        std::vector<EntityId> ids_20(ids_20_view.begin(), ids_20_view.end());

        std::vector<EntityId> mixed = {ids_10[0], ids_20[0]};
        std::unordered_set<EntityId> const id_set(mixed.begin(), mixed.end());
        std::size_t const moved = fx.source->moveByEntityIds(*fx.target, id_set, NotifyObservers::No);

        REQUIRE(moved == 2);
        REQUIRE(fx.source->getAtTime(TimeFrameIndex(10)).size() == 1);// 1 remaining
        REQUIRE(fx.source->getAtTime(TimeFrameIndex(20)).size() == 0);
        REQUIRE(fx.target->getAtTime(TimeFrameIndex(10)).size() == 1);
        REQUIRE(fx.target->getAtTime(TimeFrameIndex(20)).size() == 1);
    }

    SECTION("Move — non-existent EntityIDs") {
        std::unordered_set<EntityId> const fake = {EntityId(99999), EntityId(88888)};
        std::size_t const moved = fx.source->moveByEntityIds(*fx.target, fake, NotifyObservers::No);

        REQUIRE(moved == 0);
        REQUIRE(fx.target->getTimesWithData().empty());
        REQUIRE(fx.source->getAtTime(TimeFrameIndex(10)).size() == 2);
        REQUIRE(fx.source->getAtTime(TimeFrameIndex(20)).size() == 1);
    }
}

// ============================================================================
// Data integrity after copy / move
// ============================================================================
TEMPLATE_TEST_CASE("RaggedTimeSeries+DM - Copy/Move preserves data integrity",
                   "[ragged][entity][integrity][integration]",
                   LineData, MaskData, PointData) {

    using Traits = RaggedTestTraits<TestType>;
    EntityTestFixture<TestType> const fx;

    Traits::add(*fx.source, TimeFrameIndex(10), Traits::sample1());
    Traits::add(*fx.source, TimeFrameIndex(10), Traits::sample2());

    SECTION("Copy preserves element count and size") {
        auto ids = fx.source->getEntityIdsAtTime(TimeFrameIndex(10));
        std::unordered_set<EntityId> const id_set(ids.begin(), ids.end());
        fx.source->copyByEntityIds(*fx.target, id_set, NotifyObservers::No);

        auto src_elems = fx.source->getAtTime(TimeFrameIndex(10));
        auto tgt_elems = fx.target->getAtTime(TimeFrameIndex(10));
        REQUIRE(src_elems.size() == tgt_elems.size());
    }

    SECTION("Move preserves element count and size") {
        auto ids_view = fx.source->getEntityIdsAtTime(TimeFrameIndex(10));
        std::vector<EntityId> ids(ids_view.begin(), ids_view.end());

        // Snapshot original count
        auto original = fx.source->getAtTime(TimeFrameIndex(10));
        auto original_count = original.size();

        std::unordered_set<EntityId> const id_set(ids.begin(), ids.end());
        fx.source->moveByEntityIds(*fx.target, id_set, NotifyObservers::No);

        auto tgt_elems = fx.target->getAtTime(TimeFrameIndex(10));
        REQUIRE(tgt_elems.size() == original_count);
        REQUIRE(fx.source->getAtTime(TimeFrameIndex(10)).size() == 0);
    }
}

// ============================================================================
// Overwrite merge (mergeOverwriteTo)
// ============================================================================
TEMPLATE_TEST_CASE("RaggedTimeSeries+DM - mergeOverwriteTo",
                   "[ragged][entity][merge][integration]",
                   LineData, MaskData, PointData) {

    using Traits = RaggedTestTraits<TestType>;
    EntityTestFixture<TestType> const fx;

    SECTION("Overwrite existing time") {
        Traits::add(*fx.target, TimeFrameIndex(10), Traits::sample1());
        Traits::add(*fx.source, TimeFrameIndex(10), Traits::sample2());
        Traits::add(*fx.source, TimeFrameIndex(10), Traits::sample3());

        auto const source_ids = get_all_entity_ids(*fx.source);
        auto const merged = fx.source->mergeOverwriteTo(*fx.target, NotifyObservers::No);

        REQUIRE(merged.has_value());
        REQUIRE(*merged == 2);
        REQUIRE(fx.source->getAtTime(TimeFrameIndex(10)).size() == 2);
        REQUIRE(fx.target->getAtTime(TimeFrameIndex(10)).size() == 2);
        REQUIRE(get_all_entity_ids(*fx.target) == source_ids);
    }

    SECTION("Add new time from source") {
        Traits::add(*fx.target, TimeFrameIndex(10), Traits::sample1());
        Traits::add(*fx.source, TimeFrameIndex(30), Traits::sample2());

        auto const merged = fx.source->mergeOverwriteTo(*fx.target, NotifyObservers::No);

        REQUIRE(merged.has_value());
        REQUIRE(*merged == 1);
        REQUIRE(fx.target->getAtTime(TimeFrameIndex(10)).size() == 1);
        REQUIRE(fx.target->getAtTime(TimeFrameIndex(30)).size() == 1);
    }

    SECTION("Mixed overwrite and add") {
        Traits::add(*fx.target, TimeFrameIndex(10), Traits::sample1());
        Traits::add(*fx.target, TimeFrameIndex(20), Traits::sample2());
        Traits::add(*fx.source, TimeFrameIndex(10), Traits::sample3());
        Traits::add(*fx.source, TimeFrameIndex(30), Traits::sample1());

        auto const source_ids_10_view = fx.source->getEntityIdsAtTime(TimeFrameIndex(10));
        std::vector<EntityId> const source_ids_10(source_ids_10_view.begin(),
                                                  source_ids_10_view.end());

        auto const merged = fx.source->mergeOverwriteTo(*fx.target, NotifyObservers::No);

        REQUIRE(merged.has_value());
        REQUIRE(*merged == 2);
        REQUIRE(fx.target->getAtTime(TimeFrameIndex(10)).size() == 1);
        REQUIRE(fx.target->getAtTime(TimeFrameIndex(20)).size() == 1);
        REQUIRE(fx.target->getAtTime(TimeFrameIndex(30)).size() == 1);

        auto const target_ids_10_view = fx.target->getEntityIdsAtTime(TimeFrameIndex(10));
        std::vector<EntityId> const target_ids_10(target_ids_10_view.begin(),
                                                  target_ids_10_view.end());
        REQUIRE(target_ids_10 == source_ids_10);
    }

    SECTION("Cross-time EntityId collision") {
        Traits::add(*fx.source, TimeFrameIndex(10), Traits::sample1());
        auto const source_ids = fx.source->getEntityIdsAtTime(TimeFrameIndex(10));
        REQUIRE(source_ids.size() == 1);
        EntityId const shared_id = source_ids[0];

        fx.target->addEntryAtTime(
                TimeFrameIndex(30), Traits::sample2(), shared_id, NotifyObservers::No);

        auto const merged = fx.source->mergeOverwriteTo(*fx.target, NotifyObservers::No);

        REQUIRE(merged.has_value());
        REQUIRE(*merged == 1);
        REQUIRE(fx.target->getAtTime(TimeFrameIndex(30)).empty());
        REQUIRE(fx.target->getAtTime(TimeFrameIndex(10)).size() == 1);
        REQUIRE(get_all_entity_ids(*fx.target) == std::vector<EntityId>{shared_id});
    }

    SECTION("Empty source") {
        Traits::add(*fx.target, TimeFrameIndex(10), Traits::sample1());

        auto const merged = fx.source->mergeOverwriteTo(*fx.target, NotifyObservers::No);

        REQUIRE(merged.has_value());
        REQUIRE(*merged == 0);
        REQUIRE(fx.target->getAtTime(TimeFrameIndex(10)).size() == 1);
    }

    SECTION("TimeFrame mismatch") {
        Traits::add(*fx.source, TimeFrameIndex(10), Traits::sample1());
        Traits::add(*fx.target, TimeFrameIndex(20), Traits::sample2());

        auto const different_time_frame =
                std::make_shared<TimeFrame>(std::vector<int>{0, 10, 20, 30});
        fx.target->setTimeFrame(different_time_frame);

        auto const merged = fx.source->mergeOverwriteTo(*fx.target, NotifyObservers::No);

        REQUIRE_FALSE(merged.has_value());
        REQUIRE(fx.target->getAtTime(TimeFrameIndex(20)).size() == 1);
        REQUIRE(fx.source->getAtTime(TimeFrameIndex(10)).size() == 1);
    }

    SECTION("Self-merge rejected") {
        Traits::add(*fx.source, TimeFrameIndex(10), Traits::sample1());

        auto const merged = fx.source->mergeOverwriteTo(*fx.source, NotifyObservers::No);

        REQUIRE_FALSE(merged.has_value());
    }
}
