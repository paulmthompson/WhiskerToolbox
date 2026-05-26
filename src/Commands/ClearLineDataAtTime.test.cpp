/**
 * @file ClearLineDataAtTime.test.cpp
 * @brief Tests for ClearLineDataAtTime command
 */

#include "Commands/ClearLineDataAtTime.hpp"
#include "Commands/Core/CommandContext.hpp"
#include "Commands/Core/CommandDescriptor.hpp"
#include "Commands/Core/CommandFactory.hpp"
#include "Commands/Core/SequenceExecution.hpp"

#include "DataManager/DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "Points/Point_Data.hpp"

#include "Observer/Observer_Data.hpp"
#include "TimeController/TimeController.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

#include <catch2/catch_test_macros.hpp>

#include <memory>

#include <rfl/json.hpp>

using namespace commands;

TEST_CASE("ClearLineDataAtTime errors when data_manager is null",
          "[commands][ClearLineDataAtTime]") {
    ClearLineDataAtTime cmd(ClearLineDataAtTimeParams{.data_key = "lines", .time = 0});
    CommandContext ctx;
    ctx.data_manager = nullptr;
    auto const r = cmd.execute(ctx);
    REQUIRE_FALSE(r.success);
    REQUIRE(r.error_message.find("data_manager") != std::string::npos);
}

TEST_CASE("ClearLineDataAtTime errors when key is missing",
          "[commands][ClearLineDataAtTime]") {
    ClearLineDataAtTime cmd(ClearLineDataAtTimeParams{.data_key = "missing", .time = 1});
    auto dm = std::make_shared<DataManager>();
    CommandContext ctx;
    ctx.data_manager = dm;
    auto const r = cmd.execute(ctx);
    REQUIRE_FALSE(r.success);
    REQUIRE(r.error_message.find("missing") != std::string::npos);
}

TEST_CASE("ClearLineDataAtTime errors when key is not LineData",
          "[commands][ClearLineDataAtTime]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<PointData>("points", TimeKey("time"));
    ClearLineDataAtTime cmd(ClearLineDataAtTimeParams{.data_key = "points", .time = 0});
    CommandContext ctx;
    ctx.data_manager = dm;
    auto const r = cmd.execute(ctx);
    REQUIRE_FALSE(r.success);
    REQUIRE(r.error_message.find("points") != std::string::npos);
}

TEST_CASE("ClearLineDataAtTime clears one frame and leaves others",
          "[commands][ClearLineDataAtTime]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<LineData>("lines", TimeKey("time"));
    auto lines = dm->getData<LineData>("lines");
    lines->addEntryAtTime(TimeFrameIndex(5),
                          Line2D({1.0f, 2.0f}, {1.0f, 2.0f}),
                          EntityId(1), NotifyObservers::No);
    lines->addEntryAtTime(TimeFrameIndex(10),
                          Line2D({3.0f, 4.0f}, {3.0f, 4.0f}),
                          EntityId(2), NotifyObservers::No);
    lines->addEntryAtTime(TimeFrameIndex(20),
                          Line2D({5.0f, 6.0f}, {5.0f, 6.0f}),
                          EntityId(3), NotifyObservers::No);

    ClearLineDataAtTime clear_cmd({.data_key = "lines", .time = 10});
    CommandContext ctx;
    ctx.data_manager = dm;
    REQUIRE(clear_cmd.execute(ctx).success);

    REQUIRE(lines->getAtTime(TimeFrameIndex(5)).size() == 1);
    REQUIRE(lines->getAtTime(TimeFrameIndex(10)).empty());
    REQUIRE(lines->getAtTime(TimeFrameIndex(20)).size() == 1);
}

TEST_CASE("ClearLineDataAtTime is idempotent when frame is empty",
          "[commands][ClearLineDataAtTime]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<LineData>("lines", TimeKey("time"));

    ClearLineDataAtTime clear_cmd({.data_key = "lines", .time = 42});
    CommandContext ctx;
    ctx.data_manager = dm;
    REQUIRE(clear_cmd.execute(ctx).success);
    REQUIRE(clear_cmd.execute(ctx).success);
}

TEST_CASE("createCommand deserializes ClearLineDataAtTime",
          "[commands][ClearLineDataAtTime]") {
    auto const json = R"({"data_key": "pred_w0", "time": 42})";
    auto const generic = rfl::json::read<rfl::Generic>(json).value();
    auto cmd = createCommand("ClearLineDataAtTime", generic);
    REQUIRE(cmd != nullptr);
    REQUIRE(cmd->commandName() == "ClearLineDataAtTime");
    REQUIRE_FALSE(cmd->isUndoable());
}

TEST_CASE("executeSequence substitutes current_frame into ClearLineDataAtTime",
          "[commands][ClearLineDataAtTime][variable-substitution]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<LineData>("lines", TimeKey("time"));
    auto lines = dm->getData<LineData>("lines");
    lines->addEntryAtTime(TimeFrameIndex(9),
                          Line2D({1.0f, 2.0f}, {1.0f, 2.0f}),
                          EntityId(1), NotifyObservers::No);
    lines->addEntryAtTime(TimeFrameIndex(15),
                          Line2D({3.0f, 4.0f}, {3.0f, 4.0f}),
                          EntityId(2), NotifyObservers::No);

    CommandSequenceDescriptor seq;
    CommandDescriptor step;
    step.command_name = "ClearLineDataAtTime";
    step.parameters = rfl::json::read<rfl::Generic>(
                              R"({"data_key": "lines", "time": "${current_frame}"})")
                              .value();
    seq.commands = {step};

    CommandContext ctx;
    ctx.data_manager = dm;
    ctx.runtime_variables = {{"current_frame", "9"}};

    auto const out = executeSequence(seq, ctx);
    REQUIRE(out.result.success);
    REQUIRE(lines->getAtTime(TimeFrameIndex(9)).empty());
    REQUIRE(lines->getAtTime(TimeFrameIndex(15)).size() == 1);
}

TEST_CASE("ClearLineDataAtTime uses TimeController timeframe when present",
          "[commands][ClearLineDataAtTime]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<LineData>("lines", TimeKey("time"));
    auto lines = dm->getData<LineData>("lines");
    lines->addEntryAtTime(TimeFrameIndex(7),
                          Line2D({1.0f, 2.0f}, {1.0f, 2.0f}),
                          EntityId(1), NotifyObservers::No);

    TimeController tc;
    tc.setCurrentTime(TimePosition(TimeFrameIndex(0), dm->getTime()));

    ClearLineDataAtTime clear_cmd({.data_key = "lines", .time = 7});
    CommandContext ctx;
    ctx.data_manager = dm;
    ctx.time_controller = &tc;
    REQUIRE(clear_cmd.execute(ctx).success);
    REQUIRE(lines->getAtTime(TimeFrameIndex(7)).empty());
}
