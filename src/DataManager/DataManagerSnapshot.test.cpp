/**
 * @file DataManagerSnapshot.test.cpp
 * @brief Unit tests for DataManagerSnapshot and golden trace test infrastructure
 */

#include "DataManager/DataManagerSnapshot.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "Commands/Core/CommandContext.hpp"
#include "Commands/Core/CommandDescriptor.hpp"
#include "Commands/Core/CommandRecorder.hpp"
#include "Commands/Core/ICommand.hpp"
#include "Commands/Core/SequenceExecution.hpp"
#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"

#include <catch2/catch_test_macros.hpp>

#include <rfl/json.hpp>

using namespace commands;

// =============================================================================
// snapshotDataManager — basic tests
// =============================================================================

TEST_CASE("snapshotDataManager returns empty snapshot for default DataManager",
          "[snapshot]") {
    DataManager dm;
    auto snap = snapshotDataManager(dm);

    // Default DataManager only contains "media", which is skipped
    CHECK(snap.key_type.empty());
    CHECK(snap.key_entity_count.empty());
    CHECK(snap.key_content_hash.empty());
}

TEST_CASE("snapshotDataManager captures PointData key and count",
          "[snapshot]") {
    DataManager dm;
    dm.setData<PointData>("my_points", TimeKey("time"));
    auto pts = dm.getData<PointData>("my_points");
    pts->addAtTime(TimeFrameIndex(1),
                   std::vector<Point2D<float>>{{1.0f, 2.0f}, {3.0f, 4.0f}},
                   NotifyObservers::No);

    auto snap = snapshotDataManager(dm);

    REQUIRE(snap.key_type.count("my_points") == 1);
    CHECK(snap.key_type.at("my_points") == "points");
    CHECK(snap.key_entity_count.at("my_points") == 2);
    CHECK_FALSE(snap.key_content_hash.at("my_points").empty());
}

TEST_CASE("snapshotDataManager captures LineData key and count",
          "[snapshot]") {
    DataManager dm;
    dm.setData<LineData>("my_lines", TimeKey("time"));
    auto lines = dm.getData<LineData>("my_lines");
    lines->addEntryAtTime(TimeFrameIndex(10),
                          Line2D({1.0f, 2.0f}, {3.0f, 4.0f}),
                          EntityId(1), NotifyObservers::No);
    lines->addEntryAtTime(TimeFrameIndex(20),
                          Line2D({5.0f, 6.0f}, {7.0f, 8.0f}),
                          EntityId(2), NotifyObservers::No);

    auto snap = snapshotDataManager(dm);

    REQUIRE(snap.key_type.count("my_lines") == 1);
    CHECK(snap.key_type.at("my_lines") == "line");
    CHECK(snap.key_entity_count.at("my_lines") == 2);
}

TEST_CASE("snapshotDataManager captures DigitalIntervalSeries",
          "[snapshot]") {
    DataManager dm;
    dm.setData<DigitalIntervalSeries>("intervals", TimeKey("time"));
    auto ivs = dm.getData<DigitalIntervalSeries>("intervals");
    ivs->addEvent(Interval{10, 20});
    ivs->addEvent(Interval{30, 40});

    auto snap = snapshotDataManager(dm);

    CHECK(snap.key_type.at("intervals") == "digital_interval");
    CHECK(snap.key_entity_count.at("intervals") == 2);
}

TEST_CASE("snapshotDataManager captures DigitalEventSeries",
          "[snapshot]") {
    DataManager dm;
    dm.setData<DigitalEventSeries>("events", TimeKey("time"));
    auto evs = dm.getData<DigitalEventSeries>("events");
    evs->addEvent(TimeFrameIndex(5));
    evs->addEvent(TimeFrameIndex(15));
    evs->addEvent(TimeFrameIndex(25));

    auto snap = snapshotDataManager(dm);

    CHECK(snap.key_type.at("events") == "digital_event");
    CHECK(snap.key_entity_count.at("events") == 3);
}

TEST_CASE("snapshotDataManager captures multiple data types",
          "[snapshot]") {
    DataManager dm;
    dm.setData<PointData>("pts", TimeKey("time"));
    dm.setData<LineData>("lns", TimeKey("time"));
    dm.setData<DigitalIntervalSeries>("ivs", TimeKey("time"));

    auto snap = snapshotDataManager(dm);

    CHECK(snap.key_type.size() == 3);
    CHECK(snap.key_type.at("pts") == "points");
    CHECK(snap.key_type.at("lns") == "line");
    CHECK(snap.key_type.at("ivs") == "digital_interval");
}

// =============================================================================
// Snapshot equality and determinism
// =============================================================================

TEST_CASE("identical DataManagers produce equal snapshots",
          "[snapshot]") {
    auto make = []() {
        auto dm = std::make_unique<DataManager>();
        dm->setData<PointData>("pts", TimeKey("time"));
        auto pts = dm->getData<PointData>("pts");
        pts->addAtTime(TimeFrameIndex(1),
                       std::vector<Point2D<float>>{{1.0f, 2.0f}},
                       NotifyObservers::No);
        return dm;
    };

    auto dm1 = make();
    auto dm2 = make();

    auto snap1 = snapshotDataManager(*dm1);
    auto snap2 = snapshotDataManager(*dm2);

    CHECK(snap1 == snap2);
}

TEST_CASE("different data produces different content hashes",
          "[snapshot]") {
    DataManager dm1;
    dm1.setData<PointData>("pts", TimeKey("time"));
    dm1.getData<PointData>("pts")->addAtTime(
            TimeFrameIndex(1),
            std::vector<Point2D<float>>{{1.0f, 2.0f}},
            NotifyObservers::No);

    DataManager dm2;
    dm2.setData<PointData>("pts", TimeKey("time"));
    dm2.getData<PointData>("pts")->addAtTime(
            TimeFrameIndex(1),
            std::vector<Point2D<float>>{{99.0f, 99.0f}},
            NotifyObservers::No);

    auto snap1 = snapshotDataManager(dm1);
    auto snap2 = snapshotDataManager(dm2);

    CHECK(snap1.key_type == snap2.key_type);
    CHECK(snap1.key_entity_count == snap2.key_entity_count);
    CHECK(snap1.key_content_hash != snap2.key_content_hash);
}

TEST_CASE("empty data object produces zero count and non-empty hash",
          "[snapshot]") {
    DataManager dm;
    dm.setData<LineData>("empty_lines", TimeKey("time"));

    auto snap = snapshotDataManager(dm);

    CHECK(snap.key_entity_count.at("empty_lines") == 0);
    CHECK_FALSE(snap.key_content_hash.at("empty_lines").empty());
}

// =============================================================================
// Golden Trace Tests — replay command sequences and verify final state
// =============================================================================

TEST_CASE("golden trace: CopyByTimeRange copies line entities",
          "[snapshot][golden-trace]") {
    // Set up: DataManager with source line data
    auto dm = std::make_shared<DataManager>();
    dm->setData<LineData>("src_lines", TimeKey("time"));
    dm->setData<LineData>("dst_lines", TimeKey("time"));

    auto src = dm->getData<LineData>("src_lines");
    src->addEntryAtTime(TimeFrameIndex(10),
                        Line2D({1.0f, 2.0f}, {1.0f, 2.0f}),
                        EntityId(1), NotifyObservers::No);
    src->addEntryAtTime(TimeFrameIndex(20),
                        Line2D({3.0f, 4.0f}, {3.0f, 4.0f}),
                        EntityId(2), NotifyObservers::No);

    // Snapshot before
    auto before = snapshotDataManager(*dm);
    CHECK(before.key_entity_count.at("src_lines") == 2);
    CHECK(before.key_entity_count.at("dst_lines") == 0);

    // Define command sequence as JSON (the golden trace)
    auto const json = R"({
        "name": "copy_lines_trace",
        "commands": [
            {
                "command_name": "CopyByTimeRange",
                "parameters": {
                    "source_key": "src_lines",
                    "destination_key": "dst_lines",
                    "start_frame": 10,
                    "end_frame": 20
                }
            }
        ]
    })";

    auto seq = rfl::json::read<CommandSequenceDescriptor>(json).value();
    CommandContext ctx;
    ctx.data_manager = dm;

    auto result = executeSequence(seq, ctx);
    REQUIRE(result.result.success);

    // Snapshot after and verify
    auto after = snapshotDataManager(*dm);
    CHECK(after.key_entity_count.at("src_lines") == 2);// source unchanged
    CHECK(after.key_entity_count.at("dst_lines") == 2);// copied
    CHECK(after != before);
}

TEST_CASE("golden trace: MoveByTimeRange moves entities and changes snapshot",
          "[snapshot][golden-trace]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<LineData>("src", TimeKey("time"));
    dm->setData<LineData>("dst", TimeKey("time"));

    auto src = dm->getData<LineData>("src");
    src->addEntryAtTime(TimeFrameIndex(5),
                        Line2D({1.0f}, {1.0f}),
                        EntityId(1), NotifyObservers::No);
    src->addEntryAtTime(TimeFrameIndex(15),
                        Line2D({2.0f}, {2.0f}),
                        EntityId(2), NotifyObservers::No);
    src->addEntryAtTime(TimeFrameIndex(25),
                        Line2D({3.0f}, {3.0f}),
                        EntityId(3), NotifyObservers::No);

    auto const json = R"({
        "name": "move_partial_trace",
        "commands": [
            {
                "command_name": "MoveByTimeRange",
                "parameters": {
                    "source_key": "src",
                    "destination_key": "dst",
                    "start_frame": 5,
                    "end_frame": 15
                }
            }
        ]
    })";

    auto seq = rfl::json::read<CommandSequenceDescriptor>(json).value();
    CommandContext ctx;
    ctx.data_manager = dm;

    auto result = executeSequence(seq, ctx);
    REQUIRE(result.result.success);

    auto snap = snapshotDataManager(*dm);
    CHECK(snap.key_entity_count.at("src") == 1);// only frame 25 remains
    CHECK(snap.key_entity_count.at("dst") == 2);// frames 5 and 15 moved
}

TEST_CASE("golden trace: AddInterval creates series and adds interval",
          "[snapshot][golden-trace]") {
    auto dm = std::make_shared<DataManager>();

    auto const json = R"({
        "name": "add_interval_trace",
        "commands": [
            {
                "command_name": "AddInterval",
                "parameters": {
                    "interval_key": "tracked_regions",
                    "start_frame": 100,
                    "end_frame": 200,
                    "create_if_missing": true
                }
            },
            {
                "command_name": "AddInterval",
                "parameters": {
                    "interval_key": "tracked_regions",
                    "start_frame": 300,
                    "end_frame": 400,
                    "create_if_missing": false
                }
            }
        ]
    })";

    auto seq = rfl::json::read<CommandSequenceDescriptor>(json).value();
    CommandContext ctx;
    ctx.data_manager = dm;

    auto result = executeSequence(seq, ctx);
    REQUIRE(result.result.success);

    auto snap = snapshotDataManager(*dm);
    REQUIRE(snap.key_type.count("tracked_regions") == 1);
    CHECK(snap.key_type.at("tracked_regions") == "digital_interval");
    CHECK(snap.key_entity_count.at("tracked_regions") == 2);
}

TEST_CASE("golden trace: multi-command sequence with variable substitution",
          "[snapshot][golden-trace]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<LineData>("whisker_1", TimeKey("time"));
    dm->setData<LineData>("whisker_2", TimeKey("time"));
    dm->setData<LineData>("accepted_1", TimeKey("time"));
    dm->setData<LineData>("accepted_2", TimeKey("time"));

    // Add data to both whisker sources
    auto w1 = dm->getData<LineData>("whisker_1");
    w1->addEntryAtTime(TimeFrameIndex(10),
                       Line2D({1.0f}, {1.0f}),
                       EntityId(1), NotifyObservers::No);

    auto w2 = dm->getData<LineData>("whisker_2");
    w2->addEntryAtTime(TimeFrameIndex(10),
                       Line2D({2.0f}, {2.0f}),
                       EntityId(2), NotifyObservers::No);

    auto const json = R"({
        "name": "multi_whisker_copy",
        "variables": {
            "start": "5",
            "end": "15"
        },
        "commands": [
            {
                "command_name": "CopyByTimeRange",
                "parameters": {
                    "source_key": "whisker_1",
                    "destination_key": "accepted_1",
                    "start_frame": "${start}",
                    "end_frame": "${end}"
                }
            },
            {
                "command_name": "CopyByTimeRange",
                "parameters": {
                    "source_key": "whisker_2",
                    "destination_key": "accepted_2",
                    "start_frame": "${start}",
                    "end_frame": "${end}"
                }
            }
        ]
    })";

    auto seq = rfl::json::read<CommandSequenceDescriptor>(json).value();
    CommandContext ctx;
    ctx.data_manager = dm;

    auto result = executeSequence(seq, ctx);
    REQUIRE(result.result.success);

    auto snap = snapshotDataManager(*dm);
    CHECK(snap.key_entity_count.at("whisker_1") == 1); // unchanged
    CHECK(snap.key_entity_count.at("whisker_2") == 1); // unchanged
    CHECK(snap.key_entity_count.at("accepted_1") == 1);// copied
    CHECK(snap.key_entity_count.at("accepted_2") == 1);// copied
}

TEST_CASE("golden trace: replaying same sequence twice produces consistent snapshot",
          "[snapshot][golden-trace]") {
    auto makeAndRun = []() {
        auto dm = std::make_shared<DataManager>();
        dm->setData<LineData>("src", TimeKey("time"));
        dm->setData<LineData>("dst", TimeKey("time"));

        auto src = dm->getData<LineData>("src");
        src->addEntryAtTime(TimeFrameIndex(10),
                            Line2D({1.0f, 2.0f}, {3.0f, 4.0f}),
                            EntityId(1), NotifyObservers::No);

        auto const json = R"({
            "commands": [
                {
                    "command_name": "CopyByTimeRange",
                    "parameters": {
                        "source_key": "src",
                        "destination_key": "dst",
                        "start_frame": 0,
                        "end_frame": 100
                    }
                }
            ]
        })";

        auto seq = rfl::json::read<CommandSequenceDescriptor>(json).value();
        CommandContext ctx;
        ctx.data_manager = dm;
        executeSequence(seq, ctx);

        return snapshotDataManager(*dm);
    };

    auto snap1 = makeAndRun();
    auto snap2 = makeAndRun();
    CHECK(snap1 == snap2);
}

TEST_CASE("golden trace: CommandRecorder captures trace for snapshot verification",
          "[snapshot][golden-trace][recorder]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<LineData>("src", TimeKey("time"));
    dm->setData<LineData>("dst", TimeKey("time"));

    auto src = dm->getData<LineData>("src");
    src->addEntryAtTime(TimeFrameIndex(10),
                        Line2D({1.0f}, {2.0f}),
                        EntityId(1), NotifyObservers::No);

    // Execute with recorder
    CommandSequenceDescriptor seq;
    seq.commands.push_back(CommandDescriptor{
            .command_name = "CopyByTimeRange",
            .parameters = rfl::json::read<rfl::Generic>(R"({
                "source_key": "src",
                "destination_key": "dst",
                "start_frame": 0,
                "end_frame": 100
            })")
                                  .value(),
    });

    CommandContext ctx;
    ctx.data_manager = dm;
    CommandRecorder recorder;
    auto result = executeSequence(seq, ctx, true, &recorder);
    REQUIRE(result.result.success);

    // Re-create DataManager, replay from recorder's trace
    auto dm2 = std::make_shared<DataManager>();
    dm2->setData<LineData>("src", TimeKey("time"));
    dm2->setData<LineData>("dst", TimeKey("time"));

    auto src2 = dm2->getData<LineData>("src");
    src2->addEntryAtTime(TimeFrameIndex(10),
                         Line2D({1.0f}, {2.0f}),
                         EntityId(1), NotifyObservers::No);

    CommandContext ctx2;
    ctx2.data_manager = dm2;
    auto result2 = executeSequence(recorder.toSequence("replay"), ctx2);
    REQUIRE(result2.result.success);

    // Both DataManagers should produce identical snapshots
    auto snap1 = snapshotDataManager(*dm);
    auto snap2 = snapshotDataManager(*dm2);
    CHECK(snap1 == snap2);
}
