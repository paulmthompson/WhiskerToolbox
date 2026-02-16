#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>

#include "DataManager.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "fixtures/RaggedTimeSeriesTestTraits.hpp"
#include "fixtures/entity_id.hpp"
#include "fixtures/DataManagerFixture.hpp"

#include <algorithm>
#include <vector>
#include <ranges>

// Define the types to test
// RaggedTimeSeriesTestTraits must have specializations for these types
using RaggedTypes = std::tuple<LineData, MaskData>;

TEMPLATE_TEST_CASE("RaggedTimeSeries - Core Functionality", "[ragged][core]", LineData, MaskData) {
    using Traits = RaggedDataTestTraits<TestType>;
    using DataType = typename Traits::DataType;

    SECTION("Add and Get Data") {
        auto data = std::make_shared<DataType>();
        auto item = Traits::createSingleItem();

        data->addAtTime(TimeFrameIndex(10), item, NotifyObservers::No);

        auto retrieved = data->getAtTime(TimeFrameIndex(10));
        REQUIRE(std::ranges::distance(retrieved) == 1);
        REQUIRE(Traits::equals(*retrieved.begin(), item));

        // Add another
        data->addAtTime(TimeFrameIndex(10), item, NotifyObservers::No);
        retrieved = data->getAtTime(TimeFrameIndex(10));
        REQUIRE(std::ranges::distance(retrieved) == 2);
    }

    SECTION("Clear Data") {
        auto data = std::make_shared<DataType>();
        std::vector<int> times = {0, 10, 20};
        auto timeframe = std::make_shared<TimeFrame>(times);
        data->setTimeFrame(timeframe);

        auto item = Traits::createSingleItem();

        data->addAtTime(TimeFrameIndex(10), item, NotifyObservers::No);
        REQUIRE(std::ranges::distance(data->getAtTime(TimeFrameIndex(10))) == 1);

        bool cleared = data->clearAtTime(TimeIndexAndFrame(10, timeframe.get()), NotifyObservers::No);
        REQUIRE(cleared);
        REQUIRE(std::ranges::distance(data->getAtTime(TimeFrameIndex(10))) == 0);

        // clear empty
        cleared = data->clearAtTime(TimeIndexAndFrame(10, timeframe.get()), NotifyObservers::No);
        REQUIRE_FALSE(cleared);
    }
}

TEMPLATE_TEST_CASE_METHOD(DataManagerFixture, "RaggedTimeSeries - Copy and Move by EntityID", "[ragged][entity][copy][move]", LineData, MaskData) {
    using Traits = RaggedDataTestTraits<TestType>;
    using DataType = typename Traits::DataType;

    // DataManagerFixture provides:
    // m_data_manager (unique_ptr<DataManager>)
    // m_time_frame (shared_ptr<TimeFrame>)
    // m_times ({0, 10, 20, 30})
    // TimeKey("test_time") is already set

    SECTION("Copy by EntityID - basic functionality") {
        m_data_manager->template setData<DataType>("source", TimeKey("test_time"));
        m_data_manager->template setData<DataType>("target", TimeKey("test_time"));

        auto source = m_data_manager->template getData<DataType>("source");
        auto target = m_data_manager->template getData<DataType>("target");

        auto item = Traits::createSingleItem();

        // Add data
        source->addAtTime(TimeFrameIndex(10), item, NotifyObservers::No); // 1 item
        source->addAtTime(TimeFrameIndex(10), item, NotifyObservers::No); // 2 items
        source->addAtTime(TimeFrameIndex(20), item, NotifyObservers::No); // 1 item

        auto ids_10 = source->getEntityIdsAtTime(TimeFrameIndex(10));
        REQUIRE(std::ranges::distance(ids_10) == 2);

        std::unordered_set<EntityId> ids_to_copy;
        for(auto id : ids_10) {
            ids_to_copy.insert(id);
        }

        size_t copied = source->copyByEntityIds(*target, ids_to_copy, NotifyObservers::No);
        REQUIRE(copied == 2);

        // Check target
        REQUIRE(std::ranges::distance(target->getAtTime(TimeFrameIndex(10))) == 2);
        REQUIRE(std::ranges::distance(target->getAtTime(TimeFrameIndex(20))) == 0);

        // Check source (unchanged)
        REQUIRE(std::ranges::distance(source->getAtTime(TimeFrameIndex(10))) == 2);

        // Verify data integrity
        auto target_items = target->getAtTime(TimeFrameIndex(10));
        for(auto const& target_item : target_items) {
            REQUIRE(Traits::equals(target_item, item));
        }
    }

    SECTION("Copy by EntityID - mixed times") {
        m_data_manager->template setData<DataType>("source", TimeKey("test_time"));
        m_data_manager->template setData<DataType>("target", TimeKey("test_time"));

        auto source = m_data_manager->template getData<DataType>("source");
        auto target = m_data_manager->template getData<DataType>("target");

        auto item1 = Traits::createSingleItem();
        auto item2 = Traits::createAnotherItem();

        source->addAtTime(TimeFrameIndex(10), item1, NotifyObservers::No);
        source->addAtTime(TimeFrameIndex(20), item2, NotifyObservers::No);

        auto ids_10 = source->getEntityIdsAtTime(TimeFrameIndex(10));
        auto ids_20 = source->getEntityIdsAtTime(TimeFrameIndex(20));

        std::unordered_set<EntityId> ids_mixed;
        ids_mixed.insert(*ids_10.begin());
        ids_mixed.insert(*ids_20.begin());

        size_t copied = source->copyByEntityIds(*target, ids_mixed, NotifyObservers::No);
        REQUIRE(copied == 2);

        // Check target counts
        REQUIRE(std::ranges::distance(target->getAtTime(TimeFrameIndex(10))) == 1);
        REQUIRE(std::ranges::distance(target->getAtTime(TimeFrameIndex(20))) == 1);

        // Check data integrity
        REQUIRE(Traits::equals(*target->getAtTime(TimeFrameIndex(10)).begin(), item1));
        REQUIRE(Traits::equals(*target->getAtTime(TimeFrameIndex(20)).begin(), item2));
    }

    SECTION("Copy by EntityID - non-existent EntityIDs") {
        m_data_manager->template setData<DataType>("source", TimeKey("test_time"));
        m_data_manager->template setData<DataType>("target", TimeKey("test_time"));

        auto source = m_data_manager->template getData<DataType>("source");
        auto target = m_data_manager->template getData<DataType>("target");

        auto item = Traits::createSingleItem();
        source->addAtTime(TimeFrameIndex(10), item, NotifyObservers::No);

        std::unordered_set<EntityId> fake_ids = {EntityId(99999), EntityId(88888)};
        size_t copied = source->copyByEntityIds(*target, fake_ids, NotifyObservers::No);

        REQUIRE(copied == 0);
        REQUIRE(target->getTimesWithData().empty());
    }

    SECTION("Move by EntityID - basic functionality") {
        m_data_manager->template setData<DataType>("source", TimeKey("test_time"));
        m_data_manager->template setData<DataType>("target", TimeKey("test_time"));

        auto source = m_data_manager->template getData<DataType>("source");
        auto target = m_data_manager->template getData<DataType>("target");

        auto item = Traits::createSingleItem();

        source->addAtTime(TimeFrameIndex(10), item, NotifyObservers::No);
        source->addAtTime(TimeFrameIndex(10), item, NotifyObservers::No);

        auto ids_10_view = source->getEntityIdsAtTime(TimeFrameIndex(10));
        std::vector<EntityId> ids_10;
        for(auto id : ids_10_view) ids_10.push_back(id);

        std::unordered_set<EntityId> ids_to_move(ids_10.begin(), ids_10.end());

        size_t moved = source->moveByEntityIds(*target, ids_to_move, NotifyObservers::No);
        REQUIRE(moved == 2);

        // Check target
        REQUIRE(std::ranges::distance(target->getAtTime(TimeFrameIndex(10))) == 2);

        // Check source (removed)
        REQUIRE(std::ranges::distance(source->getAtTime(TimeFrameIndex(10))) == 0);

        // Verify integrity
        auto target_items = target->getAtTime(TimeFrameIndex(10));
        for(auto const& t_item : target_items) {
            REQUIRE(Traits::equals(t_item, item));
        }

        // Verify entity IDs preservation for Move
        auto target_ids_view = target->getEntityIdsAtTime(TimeFrameIndex(10));
        std::vector<EntityId> target_ids;
        for(auto id : target_ids_view) target_ids.push_back(id);

        std::sort(ids_10.begin(), ids_10.end());
        std::sort(target_ids.begin(), target_ids.end());
        REQUIRE(ids_10 == target_ids);
    }
}
