/**
 * @file Commands.test.cpp
 * @brief Unit tests for MoveByTimeRange, CopyByTimeRange, AddInterval, and ForEachKey commands
 */

#include "DataManager/Commands/AddInterval.hpp"
#include "DataManager/Commands/CommandContext.hpp"
#include "DataManager/Commands/CommandFactory.hpp"
#include "DataManager/Commands/CopyByTimeRange.hpp"
#include "DataManager/Commands/ForEachKey.hpp"
#include "DataManager/Commands/MoveByTimeRange.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/Lines/Line_Data.hpp"

#include <catch2/catch_test_macros.hpp>

#include <rfl/json.hpp>

using namespace commands;

// =============================================================================
// Helper: create a DataManager with two LineData objects
// =============================================================================
namespace {

CommandContext makeLineContext() {
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
    src->addEntryAtTime(TimeFrameIndex(30),
                        Line2D({5.0f, 6.0f}, {5.0f, 6.0f}),
                        EntityId(3), NotifyObservers::No);

    CommandContext ctx;
    ctx.data_manager = dm;
    return ctx;
}

}// namespace

// =============================================================================
// Factory tests for new commands
// =============================================================================

TEST_CASE("createCommand creates MoveByTimeRange from valid params",
          "[commands][factory]") {
    auto const json = R"({
        "source_key": "src", "destination_key": "dst",
        "start_frame": 10, "end_frame": 20
    })";
    auto const generic = rfl::json::read<rfl::Generic>(json).value();
    auto cmd = createCommand("MoveByTimeRange", generic);
    REQUIRE(cmd != nullptr);
    REQUIRE(cmd->commandName() == "MoveByTimeRange");
    REQUIRE(cmd->isUndoable());
}

TEST_CASE("createCommand creates CopyByTimeRange from valid params",
          "[commands][factory]") {
    auto const json = R"({
        "source_key": "src", "destination_key": "dst",
        "start_frame": 10, "end_frame": 20
    })";
    auto const generic = rfl::json::read<rfl::Generic>(json).value();
    auto cmd = createCommand("CopyByTimeRange", generic);
    REQUIRE(cmd != nullptr);
    REQUIRE(cmd->commandName() == "CopyByTimeRange");
    REQUIRE_FALSE(cmd->isUndoable());
}

TEST_CASE("createCommand creates AddInterval from valid params",
          "[commands][factory]") {
    auto const json = R"({
        "interval_key": "tracked", "start_frame": 100, "end_frame": 200,
        "create_if_missing": true
    })";
    auto const generic = rfl::json::read<rfl::Generic>(json).value();
    auto cmd = createCommand("AddInterval", generic);
    REQUIRE(cmd != nullptr);
    REQUIRE(cmd->commandName() == "AddInterval");
}

TEST_CASE("createCommand creates ForEachKey from valid params",
          "[commands][factory]") {
    auto const json = R"({
        "items": ["a", "b"],
        "variable": "key",
        "commands": [{"command_name": "AddInterval",
                      "parameters": {"interval_key": "${key}", "start_frame": 0, "end_frame": 10}}]
    })";
    auto const generic = rfl::json::read<rfl::Generic>(json).value();
    auto cmd = createCommand("ForEachKey", generic);
    REQUIRE(cmd != nullptr);
    REQUIRE(cmd->commandName() == "ForEachKey");
}

TEST_CASE("createCommand returns nullptr for invalid MoveByTimeRange params",
          "[commands][factory]") {
    auto const json = R"({"not_a_valid_field": 42})";
    auto const generic = rfl::json::read<rfl::Generic>(json).value();
    auto cmd = createCommand("MoveByTimeRange", generic);
    REQUIRE(cmd == nullptr);
}

// =============================================================================
// MoveByTimeRange tests
// =============================================================================

TEST_CASE("MoveByTimeRange moves entities between LineData objects",
          "[commands][MoveByTimeRange]") {
    auto ctx = makeLineContext();
    auto src = ctx.data_manager->getData<LineData>("src_lines");
    auto dst = ctx.data_manager->getData<LineData>("dst_lines");

    SECTION("move subset") {
        MoveByTimeRange cmd(MoveByTimeRangeParams{
                "src_lines", "dst_lines", 15, 25});
        auto result = cmd.execute(ctx);

        REQUIRE(result.success);
        REQUIRE(src->getTotalEntryCount() == 2);
        REQUIRE(dst->getTotalEntryCount() == 1);
    }

    SECTION("move all") {
        MoveByTimeRange cmd(MoveByTimeRangeParams{
                "src_lines", "dst_lines", 0, 100});
        auto result = cmd.execute(ctx);

        REQUIRE(result.success);
        REQUIRE(src->getTotalEntryCount() == 0);
        REQUIRE(dst->getTotalEntryCount() == 3);
    }

    SECTION("move nothing when range has no entities") {
        MoveByTimeRange cmd(MoveByTimeRangeParams{
                "src_lines", "dst_lines", 50, 60});
        auto result = cmd.execute(ctx);

        REQUIRE(result.success);
        REQUIRE(src->getTotalEntryCount() == 3);
        REQUIRE(dst->getTotalEntryCount() == 0);
    }

    SECTION("error when source key missing") {
        MoveByTimeRange cmd(MoveByTimeRangeParams{
                "nonexistent", "dst_lines", 0, 100});
        auto result = cmd.execute(ctx);

        REQUIRE_FALSE(result.success);
    }

    SECTION("error when destination key missing") {
        MoveByTimeRange cmd(MoveByTimeRangeParams{
                "src_lines", "nonexistent", 0, 100});
        auto result = cmd.execute(ctx);

        REQUIRE_FALSE(result.success);
    }
}

TEST_CASE("MoveByTimeRange errors on type mismatch",
          "[commands][MoveByTimeRange]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<LineData>("lines", TimeKey("time"));
    dm->setData<DigitalIntervalSeries>("intervals", TimeKey("time"));

    CommandContext ctx;
    ctx.data_manager = dm;

    MoveByTimeRange cmd(MoveByTimeRangeParams{
            "lines", "intervals", 0, 100});
    auto result = cmd.execute(ctx);
    REQUIRE_FALSE(result.success);
    REQUIRE(result.error_message.find("same data type") != std::string::npos);
}

TEST_CASE("MoveByTimeRange supports undo",
          "[commands][MoveByTimeRange]") {
    auto ctx = makeLineContext();
    auto src = ctx.data_manager->getData<LineData>("src_lines");
    auto dst = ctx.data_manager->getData<LineData>("dst_lines");

    MoveByTimeRange cmd(MoveByTimeRangeParams{
            "src_lines", "dst_lines", 15, 25});

    auto result = cmd.execute(ctx);
    REQUIRE(result.success);
    REQUIRE(src->getTotalEntryCount() == 2);
    REQUIRE(dst->getTotalEntryCount() == 1);

    auto undo_result = cmd.undo(ctx);
    REQUIRE(undo_result.success);
    REQUIRE(src->getTotalEntryCount() == 3);
    REQUIRE(dst->getTotalEntryCount() == 0);
}

TEST_CASE("MoveByTimeRange with DigitalEventSeries",
          "[commands][MoveByTimeRange]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<DigitalEventSeries>("src_events", TimeKey("time"));
    dm->setData<DigitalEventSeries>("dst_events", TimeKey("time"));

    auto src = dm->getData<DigitalEventSeries>("src_events");
    src->addEvent(TimeFrameIndex(10), EntityId(1), NotifyObservers::No);
    src->addEvent(TimeFrameIndex(20), EntityId(2), NotifyObservers::No);
    src->addEvent(TimeFrameIndex(30), EntityId(3), NotifyObservers::No);

    CommandContext ctx;
    ctx.data_manager = dm;

    MoveByTimeRange cmd(MoveByTimeRangeParams{
            "src_events", "dst_events", 15, 25});
    auto result = cmd.execute(ctx);
    REQUIRE(result.success);
    REQUIRE(src->size() == 2);
    REQUIRE(dm->getData<DigitalEventSeries>("dst_events")->size() == 1);
}

TEST_CASE("MoveByTimeRange toJson round-trip",
          "[commands][MoveByTimeRange]") {
    MoveByTimeRange cmd(MoveByTimeRangeParams{
            "src", "dst", 100, 200});
    auto const json = cmd.toJson();
    auto const parsed = rfl::json::read<MoveByTimeRangeParams>(json);
    REQUIRE(parsed);
    CHECK(parsed.value().source_key == "src");
    CHECK(parsed.value().destination_key == "dst");
    CHECK(parsed.value().start_frame == 100);
    CHECK(parsed.value().end_frame == 200);
}

// =============================================================================
// CopyByTimeRange tests
// =============================================================================

TEST_CASE("CopyByTimeRange copies entities between LineData objects",
          "[commands][CopyByTimeRange]") {
    auto ctx = makeLineContext();
    auto src = ctx.data_manager->getData<LineData>("src_lines");
    auto dst = ctx.data_manager->getData<LineData>("dst_lines");

    SECTION("copy subset") {
        CopyByTimeRange cmd(CopyByTimeRangeParams{
                "src_lines", "dst_lines", 15, 25});
        auto result = cmd.execute(ctx);

        REQUIRE(result.success);
        REQUIRE(src->getTotalEntryCount() == 3);// source unchanged
        REQUIRE(dst->getTotalEntryCount() == 1);
    }

    SECTION("copy all") {
        CopyByTimeRange cmd(CopyByTimeRangeParams{
                "src_lines", "dst_lines", 0, 100});
        auto result = cmd.execute(ctx);

        REQUIRE(result.success);
        REQUIRE(src->getTotalEntryCount() == 3);// source unchanged
        REQUIRE(dst->getTotalEntryCount() == 3);
    }

    SECTION("copy nothing") {
        CopyByTimeRange cmd(CopyByTimeRangeParams{
                "src_lines", "dst_lines", 50, 60});
        auto result = cmd.execute(ctx);

        REQUIRE(result.success);
        REQUIRE(src->getTotalEntryCount() == 3);
        REQUIRE(dst->getTotalEntryCount() == 0);
    }
}

TEST_CASE("CopyByTimeRange with DigitalIntervalSeries",
          "[commands][CopyByTimeRange]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<DigitalIntervalSeries>("src_intervals", TimeKey("time"));
    dm->setData<DigitalIntervalSeries>("dst_intervals", TimeKey("time"));

    auto src = dm->getData<DigitalIntervalSeries>("src_intervals");
    src->addEvent(TimeFrameIndex(10), TimeFrameIndex(20));
    src->addEvent(TimeFrameIndex(30), TimeFrameIndex(40));

    CommandContext ctx;
    ctx.data_manager = dm;

    CopyByTimeRange cmd(CopyByTimeRangeParams{
            "src_intervals", "dst_intervals", 5, 25});
    auto result = cmd.execute(ctx);
    REQUIRE(result.success);
    // Source unchanged
    REQUIRE(src->size() == 2);
    // Destination has the overlapping interval
    REQUIRE(dm->getData<DigitalIntervalSeries>("dst_intervals")->size() == 1);
}

TEST_CASE("CopyByTimeRange errors on missing key",
          "[commands][CopyByTimeRange]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<LineData>("src", TimeKey("time"));

    CommandContext ctx;
    ctx.data_manager = dm;

    CopyByTimeRange cmd(CopyByTimeRangeParams{"src", "missing", 0, 100});
    auto result = cmd.execute(ctx);
    REQUIRE_FALSE(result.success);
}

TEST_CASE("CopyByTimeRange toJson round-trip",
          "[commands][CopyByTimeRange]") {
    CopyByTimeRange cmd(CopyByTimeRangeParams{
            "src", "dst", 100, 200});
    auto const json = cmd.toJson();
    auto const parsed = rfl::json::read<CopyByTimeRangeParams>(json);
    REQUIRE(parsed);
    CHECK(parsed.value().source_key == "src");
    CHECK(parsed.value().destination_key == "dst");
    CHECK(parsed.value().start_frame == 100);
    CHECK(parsed.value().end_frame == 200);
}

// =============================================================================
// AddInterval tests
// =============================================================================

TEST_CASE("AddInterval appends to existing series",
          "[commands][AddInterval]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<DigitalIntervalSeries>("tracked", TimeKey("time"));

    CommandContext ctx;
    ctx.data_manager = dm;

    AddInterval cmd(AddIntervalParams{"tracked", 100, 200, false});
    auto result = cmd.execute(ctx);

    REQUIRE(result.success);
    auto series = dm->getData<DigitalIntervalSeries>("tracked");
    REQUIRE(series->size() == 1);
}

TEST_CASE("AddInterval creates series when create_if_missing is true",
          "[commands][AddInterval]") {
    auto dm = std::make_shared<DataManager>();

    CommandContext ctx;
    ctx.data_manager = dm;

    AddInterval cmd(AddIntervalParams{"new_series", 100, 200, true});
    auto result = cmd.execute(ctx);

    REQUIRE(result.success);
    auto series = dm->getData<DigitalIntervalSeries>("new_series");
    REQUIRE(series != nullptr);
    REQUIRE(series->size() == 1);
}

TEST_CASE("AddInterval errors when series missing and create_if_missing is false",
          "[commands][AddInterval]") {
    auto dm = std::make_shared<DataManager>();

    CommandContext ctx;
    ctx.data_manager = dm;

    AddInterval cmd(AddIntervalParams{"missing", 100, 200, false});
    auto result = cmd.execute(ctx);

    REQUIRE_FALSE(result.success);
    REQUIRE(result.error_message.find("not found") != std::string::npos);
}

TEST_CASE("AddInterval multiple intervals accumulate",
          "[commands][AddInterval]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<DigitalIntervalSeries>("tracked", TimeKey("time"));

    CommandContext ctx;
    ctx.data_manager = dm;

    AddInterval cmd1(AddIntervalParams{"tracked", 100, 200, false});
    AddInterval cmd2(AddIntervalParams{"tracked", 300, 400, false});

    REQUIRE(cmd1.execute(ctx).success);
    REQUIRE(cmd2.execute(ctx).success);

    auto series = dm->getData<DigitalIntervalSeries>("tracked");
    REQUIRE(series->size() == 2);
}

TEST_CASE("AddInterval toJson round-trip",
          "[commands][AddInterval]") {
    AddInterval cmd(AddIntervalParams{"tracked", 100, 200, true});
    auto const json = cmd.toJson();
    auto const parsed = rfl::json::read<AddIntervalParams>(json);
    REQUIRE(parsed);
    CHECK(parsed.value().interval_key == "tracked");
    CHECK(parsed.value().start_frame == 100);
    CHECK(parsed.value().end_frame == 200);
    CHECK(parsed.value().create_if_missing == true);
}

// =============================================================================
// ForEachKey tests
// =============================================================================

TEST_CASE("ForEachKey executes sub-commands for each item",
          "[commands][ForEachKey]") {
    auto dm = std::make_shared<DataManager>();

    CommandContext ctx;
    ctx.data_manager = dm;

    // ForEachKey with AddInterval sub-commands, creating series per item
    CommandDescriptor add_desc;
    add_desc.command_name = "AddInterval";
    auto const params_json = R"({
        "interval_key": "region_${key}",
        "start_frame": 100, "end_frame": 200, "create_if_missing": true
    })";
    add_desc.parameters = rfl::json::read<rfl::Generic>(params_json).value();

    ForEachKey cmd(ForEachKeyParams{
            {"alpha", "beta", "gamma"},
            "key",
            {add_desc}});

    auto result = cmd.execute(ctx);
    REQUIRE(result.success);

    // Each item should have created its own interval series
    REQUIRE(dm->getData<DigitalIntervalSeries>("region_alpha") != nullptr);
    REQUIRE(dm->getData<DigitalIntervalSeries>("region_beta") != nullptr);
    REQUIRE(dm->getData<DigitalIntervalSeries>("region_gamma") != nullptr);
}

TEST_CASE("ForEachKey with empty items does nothing",
          "[commands][ForEachKey]") {
    auto dm = std::make_shared<DataManager>();

    CommandContext ctx;
    ctx.data_manager = dm;

    CommandDescriptor desc;
    desc.command_name = "AddInterval";
    auto const pjson = R"({"interval_key": "x", "start_frame": 0, "end_frame": 1})";
    desc.parameters = rfl::json::read<rfl::Generic>(pjson).value();

    ForEachKey cmd(ForEachKeyParams{{}, "key", {desc}});
    auto result = cmd.execute(ctx);
    REQUIRE(result.success);
}

TEST_CASE("ForEachKey stops on sub-command error",
          "[commands][ForEachKey]") {
    auto dm = std::make_shared<DataManager>();

    CommandContext ctx;
    ctx.data_manager = dm;

    // AddInterval with create_if_missing=false will fail when series doesn't exist
    CommandDescriptor desc;
    desc.command_name = "AddInterval";
    auto const pjson = R"({
        "interval_key": "missing_${key}",
        "start_frame": 0, "end_frame": 1, "create_if_missing": false
    })";
    desc.parameters = rfl::json::read<rfl::Generic>(pjson).value();

    ForEachKey cmd(ForEachKeyParams{{"a", "b"}, "key", {desc}});
    auto result = cmd.execute(ctx);
    REQUIRE_FALSE(result.success);
}

TEST_CASE("ForEachKey errors on unknown sub-command",
          "[commands][ForEachKey]") {
    auto dm = std::make_shared<DataManager>();

    CommandContext ctx;
    ctx.data_manager = dm;

    CommandDescriptor desc;
    desc.command_name = "NoSuchCommand";
    desc.parameters = rfl::json::read<rfl::Generic>(R"({})").value();

    ForEachKey cmd(ForEachKeyParams{{"x"}, "key", {desc}});
    auto result = cmd.execute(ctx);
    REQUIRE_FALSE(result.success);
    REQUIRE(result.error_message.find("Unknown sub-command") != std::string::npos);
}

TEST_CASE("ForEachKey undo reverses sub-commands",
          "[commands][ForEachKey]") {
    auto dm = std::make_shared<DataManager>();
    // Create source and destination for move commands
    dm->setData<LineData>("pred_alpha", TimeKey("time"));
    dm->setData<LineData>("gt_alpha", TimeKey("time"));
    dm->setData<LineData>("pred_beta", TimeKey("time"));
    dm->setData<LineData>("gt_beta", TimeKey("time"));

    auto pred_a = dm->getData<LineData>("pred_alpha");
    pred_a->addEntryAtTime(TimeFrameIndex(10),
                           Line2D({1.0f}, {1.0f}),
                           EntityId(1), NotifyObservers::No);

    auto pred_b = dm->getData<LineData>("pred_beta");
    pred_b->addEntryAtTime(TimeFrameIndex(10),
                           Line2D({2.0f}, {2.0f}),
                           EntityId(2), NotifyObservers::No);

    CommandContext ctx;
    ctx.data_manager = dm;

    CommandDescriptor move_desc;
    move_desc.command_name = "MoveByTimeRange";
    auto const pjson = R"({
        "source_key": "pred_${item}",
        "destination_key": "gt_${item}",
        "start_frame": 0, "end_frame": 100
    })";
    move_desc.parameters = rfl::json::read<rfl::Generic>(pjson).value();

    ForEachKey cmd(ForEachKeyParams{{"alpha", "beta"}, "item", {move_desc}});

    auto result = cmd.execute(ctx);
    REQUIRE(result.success);
    REQUIRE(pred_a->getTotalEntryCount() == 0);
    REQUIRE(pred_b->getTotalEntryCount() == 0);
    REQUIRE(dm->getData<LineData>("gt_alpha")->getTotalEntryCount() == 1);
    REQUIRE(dm->getData<LineData>("gt_beta")->getTotalEntryCount() == 1);

    // Undo should move them back
    REQUIRE(cmd.isUndoable());
    auto undo_result = cmd.undo(ctx);
    REQUIRE(undo_result.success);
    REQUIRE(pred_a->getTotalEntryCount() == 1);
    REQUIRE(pred_b->getTotalEntryCount() == 1);
    REQUIRE(dm->getData<LineData>("gt_alpha")->getTotalEntryCount() == 0);
    REQUIRE(dm->getData<LineData>("gt_beta")->getTotalEntryCount() == 0);
}

TEST_CASE("ForEachKey toJson round-trip",
          "[commands][ForEachKey]") {
    CommandDescriptor desc;
    desc.command_name = "AddInterval";
    desc.parameters = rfl::json::read<rfl::Generic>(
                              R"({"interval_key": "x", "start_frame": 0, "end_frame": 1})")
                              .value();

    ForEachKey cmd(ForEachKeyParams{{"a", "b"}, "var", {desc}});
    auto const json = cmd.toJson();
    auto const parsed = rfl::json::read<ForEachKeyParams>(json);
    REQUIRE(parsed);
    CHECK(parsed.value().items.size() == 2);
    CHECK(parsed.value().variable == "var");
    CHECK(parsed.value().commands.size() == 1);
}
