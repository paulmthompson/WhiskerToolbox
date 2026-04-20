/**
 * @file SetEventAtTime.test.cpp
 * @brief Tests for SetEventAtTime, FlipEventAtTime, and triage-style command sequences
 */

#include "Commands/SetEventAtTime.hpp"
#include "Commands/AdvanceFrame.hpp"
#include "Commands/Core/CommandContext.hpp"
#include "Commands/Core/CommandDescriptor.hpp"
#include "Commands/Core/CommandFactory.hpp"
#include "Commands/Core/SequenceExecution.hpp"
#include "Commands/FlipEventAtTime.hpp"

#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"

#include "TimeController/TimeController.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <catch2/catch_test_macros.hpp>

#include <memory>

#include <rfl/json.hpp>

using namespace commands;

TEST_CASE("SetEventAtTime errors when data_manager is null", "[commands][SetEventAtTime]") {
    SetEventAtTime cmd(SetEventAtTimeParams{.data_key = "x", .time = 0, .event = true});
    CommandContext ctx;
    ctx.data_manager = nullptr;
    auto const r = cmd.execute(ctx);
    REQUIRE_FALSE(r.success);
    REQUIRE(r.error_message.find("data_manager") != std::string::npos);
}

TEST_CASE("SetEventAtTime errors when key is missing", "[commands][SetEventAtTime]") {
    SetEventAtTime cmd(SetEventAtTimeParams{.data_key = "missing", .time = 1, .event = true});
    auto dm = std::make_shared<DataManager>();
    CommandContext ctx;
    ctx.data_manager = dm;
    auto const r = cmd.execute(ctx);
    REQUIRE_FALSE(r.success);
    REQUIRE(r.error_message.find("missing") != std::string::npos);
}

TEST_CASE("SetEventAtTime sets and clears a one-frame interval", "[commands][SetEventAtTime]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<DigitalIntervalSeries>("tracked", TimeKey("time"));
    auto series = dm->getData<DigitalIntervalSeries>("tracked");
    auto const tf = dm->getTime();
    REQUIRE(tf);

    SetEventAtTime set_on({.data_key = "tracked", .time = 7, .event = true});
    CommandContext ctx_on{.data_manager = dm};
    REQUIRE(set_on.execute(ctx_on).success);
    REQUIRE(series->hasIntervalAtTime(TimeFrameIndex(7), *tf));

    SetEventAtTime set_off({.data_key = "tracked", .time = 7, .event = false});
    REQUIRE(set_off.execute(ctx_on).success);
    REQUIRE_FALSE(series->hasIntervalAtTime(TimeFrameIndex(7), *tf));
}

TEST_CASE("createCommand deserializes SetEventAtTime", "[commands][SetEventAtTime]") {
    auto const json = R"({"data_key": "contact", "time": 42, "event": true})";
    auto const generic = rfl::json::read<rfl::Generic>(json).value();
    auto cmd = createCommand("SetEventAtTime", generic);
    REQUIRE(cmd != nullptr);
    REQUIRE(cmd->commandName() == "SetEventAtTime");
}

TEST_CASE("FlipEventAtTime toggles membership twice", "[commands][FlipEventAtTime]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<DigitalIntervalSeries>("contact", TimeKey("time"));
    auto series = dm->getData<DigitalIntervalSeries>("contact");
    auto const tf = dm->getTime();
    REQUIRE(tf);
    CommandContext ctx{.data_manager = dm};

    FlipEventAtTime flip({.data_key = "contact", .time = 3});
    REQUIRE(flip.execute(ctx).success);
    REQUIRE(series->hasIntervalAtTime(TimeFrameIndex(3), *tf));

    REQUIRE(flip.execute(ctx).success);
    REQUIRE_FALSE(series->hasIntervalAtTime(TimeFrameIndex(3), *tf));
}

TEST_CASE("executeSequence substitutes current_frame into SetEventAtTime",
          "[commands][SetEventAtTime][variable-substitution]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<DigitalIntervalSeries>("contact", TimeKey("time"));
    auto series = dm->getData<DigitalIntervalSeries>("contact");
    auto const tf = dm->getTime();
    REQUIRE(tf);

    CommandSequenceDescriptor seq;
    CommandDescriptor step;
    step.command_name = "SetEventAtTime";
    step.parameters = rfl::json::read<rfl::Generic>(
                              R"({"data_key": "contact", "time": "${current_frame}", "event": true})")
                              .value();
    seq.commands = {step};

    CommandContext ctx;
    ctx.data_manager = dm;
    ctx.runtime_variables = {{"current_frame", "9"}};

    auto const out = executeSequence(seq, ctx);
    REQUIRE(out.result.success);
    REQUIRE(series->hasIntervalAtTime(TimeFrameIndex(9), *tf));
}

TEST_CASE("executeSequence mark contact tracked and advance", "[commands][triage-sequence]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<DigitalIntervalSeries>("contact", TimeKey("time"));
    dm->setData<DigitalIntervalSeries>("tracked_regions", TimeKey("time"));
    auto contact = dm->getData<DigitalIntervalSeries>("contact");
    auto tracked = dm->getData<DigitalIntervalSeries>("tracked_regions");
    auto const tf = dm->getTime();
    REQUIRE(tf);

    TimeController tc;
    auto const dm_tf = dm->getTime();
    tc.setCurrentTime(TimePosition(TimeFrameIndex(10), dm_tf));

    auto const json_str = R"({
        "name": "mark_contact_and_advance",
        "commands": [
            { "command_name": "SetEventAtTime",
              "parameters": { "data_key": "contact", "time": "${current_frame}", "event": true }},
            { "command_name": "SetEventAtTime",
              "parameters": { "data_key": "tracked_regions", "time": "${current_frame}", "event": true }},
            { "command_name": "AdvanceFrame",
              "parameters": { "delta": 1 }}
        ]
    })";

    auto seq = rfl::json::read<CommandSequenceDescriptor>(json_str);
    REQUIRE(seq);

    CommandContext ctx;
    ctx.data_manager = dm;
    ctx.time_controller = &tc;
    ctx.runtime_variables = {{"current_frame", "10"}};

    auto const out = executeSequence(seq.value(), ctx);
    REQUIRE(out.result.success);
    REQUIRE(contact->hasIntervalAtTime(TimeFrameIndex(10), *tf));
    REQUIRE(tracked->hasIntervalAtTime(TimeFrameIndex(10), *tf));
    REQUIRE(tc.currentTimeIndex() == TimeFrameIndex(11));
}
