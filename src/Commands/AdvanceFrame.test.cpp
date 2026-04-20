/**
 * @file AdvanceFrame.test.cpp
 * @brief Tests for AdvanceFrame command and sequence execution with TimeController
 */

#include "Commands/AdvanceFrame.hpp"
#include "Commands/Core/CommandContext.hpp"
#include "Commands/Core/CommandDescriptor.hpp"
#include "Commands/Core/CommandFactory.hpp"
#include "Commands/Core/SequenceExecution.hpp"

#include "TimeController/TimeController.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>

#include <memory>

#include <rfl/json.hpp>

using namespace commands;

TEST_CASE("AdvanceFrame errors when time_controller is null", "[commands][AdvanceFrame]") {
    AdvanceFrame cmd(AdvanceFrameParams{.delta = 1});
    CommandContext ctx;
    ctx.time_controller = nullptr;
    auto const r = cmd.execute(ctx);
    REQUIRE_FALSE(r.success);
    REQUIRE(r.error_message.find("time_controller") != std::string::npos);
}

TEST_CASE("createCommand creates AdvanceFrame from valid params", "[commands][AdvanceFrame]") {
    auto const json = R"({"delta": 3})";
    auto const generic = rfl::json::read<rfl::Generic>(json).value();
    auto cmd = createCommand("AdvanceFrame", generic);
    REQUIRE(cmd != nullptr);
    REQUIRE(cmd->commandName() == "AdvanceFrame");
}

TEST_CASE("AdvanceFrame shifts current index by delta", "[commands][AdvanceFrame]") {
    AdvanceFrame cmd(AdvanceFrameParams{.delta = -2});
    TimeController tc;
    auto const tf = std::make_shared<TimeFrame>();
    tc.setCurrentTime(TimePosition(TimeFrameIndex(5), tf));
    CommandContext ctx;
    ctx.time_controller = &tc;
    auto const r = cmd.execute(ctx);
    REQUIRE(r.success);
    REQUIRE(tc.currentTimeIndex() == TimeFrameIndex(3));
}

TEST_CASE("executeSequence runs two AdvanceFrame steps", "[commands][AdvanceFrame]") {
    TimeController tc;
    auto const tf = std::make_shared<TimeFrame>();
    tc.setCurrentTime(TimePosition(TimeFrameIndex(100), tf));

    CommandSequenceDescriptor seq;
    CommandDescriptor step1;
    step1.command_name = "AdvanceFrame";
    step1.parameters = rfl::json::read<rfl::Generic>(R"({"delta": 1})").value();
    CommandDescriptor step2;
    step2.command_name = "AdvanceFrame";
    step2.parameters = rfl::json::read<rfl::Generic>(R"({"delta": 2})").value();
    seq.commands = {step1, step2};

    CommandContext ctx;
    ctx.time_controller = &tc;
    auto const out = executeSequence(seq, ctx);
    REQUIRE(out.result.success);
    REQUIRE(tc.currentTimeIndex() == TimeFrameIndex(103));
}
