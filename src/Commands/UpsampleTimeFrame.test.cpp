/**
 * @file UpsampleTimeFrame.test.cpp
 * @brief Tests for UpsampleTimeFrame command
 */

#include "Commands/UpsampleTimeFrame.hpp"
#include "Commands/Core/CommandContext.hpp"
#include "Commands/Core/CommandFactory.hpp"
#include "Commands/Core/CommandRegistry.hpp"

#include "DataManager/DataManager.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

#include <rfl/DefaultIfMissing.hpp>
#include <rfl/json.hpp>

using namespace commands;

namespace {

std::shared_ptr<DataManager> makeManagerWithSourceTimeFrame(
        std::string const & source_key,
        std::vector<int> const & times) {
    auto dm = std::make_shared<DataManager>();
    dm->setTime(TimeKey(source_key), std::make_shared<TimeFrame>(times), true);
    return dm;
}

}// namespace

TEST_CASE("UpsampleTimeFrame creates upsampled TimeFrame", "[commands][UpsampleTimeFrame]") {
    auto const dm = makeManagerWithSourceTimeFrame("camera_time", {0, 60, 120});

    UpsampleTimeFrame cmd(UpsampleTimeFrameParams{
            .source_time_key = "camera_time",
            .output_time_key = "camera_time_4x",
            .upsampling_factor = 4,
            .overwrite = false});

    CommandContext ctx;
    ctx.data_manager = dm;
    auto const result = cmd.execute(ctx);

    REQUIRE(result.success);
    REQUIRE(result.affected_keys == std::vector<std::string>{"camera_time_4x"});

    auto const output_tf = dm->getTime(TimeKey("camera_time_4x"));
    REQUIRE(output_tf != nullptr);
    REQUIRE(output_tf->getTotalFrameCount() == 9);
    CHECK(output_tf->getTimeAtIndex(TimeFrameIndex(0)) == 0);
    CHECK(output_tf->getTimeAtIndex(TimeFrameIndex(4)) == 60);
    CHECK(output_tf->getTimeAtIndex(TimeFrameIndex(8)) == 120);
}

TEST_CASE("UpsampleTimeFrame errors when source TimeFrame is missing", "[commands][UpsampleTimeFrame]") {
    auto const dm = std::make_shared<DataManager>();

    UpsampleTimeFrame cmd(UpsampleTimeFrameParams{
            .source_time_key = "missing",
            .output_time_key = "camera_time_2x",
            .upsampling_factor = 2});

    CommandContext ctx;
    ctx.data_manager = dm;
    auto const result = cmd.execute(ctx);

    REQUIRE_FALSE(result.success);
    REQUIRE(result.error_message.find("missing") != std::string::npos);
}

TEST_CASE("UpsampleTimeFrame errors on duplicate output key", "[commands][UpsampleTimeFrame]") {
    auto const dm = makeManagerWithSourceTimeFrame("camera_time", {0, 60, 120});
    dm->setTime(TimeKey("camera_time_2x"), std::make_shared<TimeFrame>(std::vector<int>{0}), true);

    UpsampleTimeFrame cmd(UpsampleTimeFrameParams{
            .source_time_key = "camera_time",
            .output_time_key = "camera_time_2x",
            .upsampling_factor = 2,
            .overwrite = false});

    CommandContext ctx;
    ctx.data_manager = dm;
    auto const result = cmd.execute(ctx);

    REQUIRE_FALSE(result.success);
    REQUIRE(result.error_message.find("already exist") != std::string::npos);
}

TEST_CASE("UpsampleTimeFrame overwrite replaces existing output key", "[commands][UpsampleTimeFrame]") {
    auto const dm = makeManagerWithSourceTimeFrame("camera_time", {100, 200});
    dm->setTime(TimeKey("camera_time_2x"), std::make_shared<TimeFrame>(std::vector<int>{0}), true);

    UpsampleTimeFrame cmd(UpsampleTimeFrameParams{
            .source_time_key = "camera_time",
            .output_time_key = "camera_time_2x",
            .upsampling_factor = 2,
            .overwrite = true});

    CommandContext ctx;
    ctx.data_manager = dm;
    auto const result = cmd.execute(ctx);

    REQUIRE(result.success);

    auto const output_tf = dm->getTime(TimeKey("camera_time_2x"));
    REQUIRE(output_tf != nullptr);
    REQUIRE(output_tf->getTotalFrameCount() == 3);
    CHECK(output_tf->getTimeAtIndex(TimeFrameIndex(1)) == 150);
}

TEST_CASE("UpsampleTimeFrame errors on invalid factor", "[commands][UpsampleTimeFrame]") {
    auto const dm = makeManagerWithSourceTimeFrame("camera_time", {0, 60});

    UpsampleTimeFrame cmd(UpsampleTimeFrameParams{
            .source_time_key = "camera_time",
            .output_time_key = "camera_time_0x",
            .upsampling_factor = 0});

    CommandContext ctx;
    ctx.data_manager = dm;
    auto const result = cmd.execute(ctx);

    REQUIRE_FALSE(result.success);
    REQUIRE(result.error_message.find("upsampling_factor") != std::string::npos);
}

TEST_CASE("UpsampleTimeFrameParams applies DefaultIfMissing for omitted fields",
          "[commands][UpsampleTimeFrame]") {
    auto const json = R"({"source_time_key":"camera_time","output_time_key":"camera_time_4x","upsampling_factor":4})";
    auto const parsed = rfl::json::read<UpsampleTimeFrameParams, rfl::DefaultIfMissing>(json);
    REQUIRE(parsed);
    auto const & val = parsed.value();
    CHECK(val.source_time_key == "camera_time");
    CHECK(val.output_time_key == "camera_time_4x");
    CHECK(val.upsampling_factor == 4);
    CHECK_FALSE(val.overwrite);
}

TEST_CASE("createCommand creates UpsampleTimeFrame from valid params", "[commands][UpsampleTimeFrame]") {
    REQUIRE(isKnownCommandName("UpsampleTimeFrame"));

    auto const json = R"({"source_time_key":"camera_time","output_time_key":"camera_time_4x","upsampling_factor":4})";
    auto cmd = CommandRegistry::instance().create("UpsampleTimeFrame", json);
    REQUIRE(cmd != nullptr);
    REQUIRE(cmd->commandName() == "UpsampleTimeFrame");
}
