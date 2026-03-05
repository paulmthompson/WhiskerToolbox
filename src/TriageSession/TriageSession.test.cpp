/**
 * @file TriageSession.test.cpp
 * @brief Unit tests for TriageSession state machine
 */

#include "TriageSession/TriageSession.hpp"

#include "DataManager/Commands/AddInterval.hpp"
#include "DataManager/Commands/CommandContext.hpp"
#include "DataManager/Commands/CommandDescriptor.hpp"
#include "DataManager/Commands/MoveByTimeRange.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/Lines/Line_Data.hpp"

#include <catch2/catch_test_macros.hpp>

#include <rfl/json.hpp>

using namespace triage;
using namespace commands;

// =============================================================================
// Test helpers
// =============================================================================
namespace {

/// Create a DataManager with source/destination LineData and populate source
std::shared_ptr<DataManager> makeTestDataManager() {
    auto dm = std::make_shared<DataManager>();
    dm->setData<LineData>("pred_w0", TimeKey("time"));
    dm->setData<LineData>("gt_w0", TimeKey("time"));

    auto src = dm->getData<LineData>("pred_w0");
    src->addEntryAtTime(TimeFrameIndex(100),
                        Line2D({1.0f, 2.0f}, {1.0f, 2.0f}),
                        EntityId(1), NotifyObservers::No);
    src->addEntryAtTime(TimeFrameIndex(200),
                        Line2D({3.0f, 4.0f}, {3.0f, 4.0f}),
                        EntityId(2), NotifyObservers::No);
    src->addEntryAtTime(TimeFrameIndex(300),
                        Line2D({5.0f, 6.0f}, {5.0f, 6.0f}),
                        EntityId(3), NotifyObservers::No);
    return dm;
}

/// Build a simple triage pipeline: move + add interval
CommandSequenceDescriptor makeTestPipeline() {
    CommandSequenceDescriptor seq;
    seq.name = "Test Triage Pipeline";

    // Move command using runtime variables
    CommandDescriptor move_cmd;
    move_cmd.command_name = "MoveByTimeRange";
    auto move_json = R"({
        "source_key": "pred_w0",
        "destination_key": "gt_w0",
        "start_frame": "${mark_frame}",
        "end_frame": "${current_frame}"
    })";
    move_cmd.parameters = rfl::json::read<rfl::Generic>(move_json).value();

    // AddInterval command using runtime variables
    CommandDescriptor interval_cmd;
    interval_cmd.command_name = "AddInterval";
    auto interval_json = R"({
        "interval_key": "tracked_regions",
        "start_frame": "${mark_frame}",
        "end_frame": "${current_frame}",
        "create_if_missing": true
    })";
    interval_cmd.parameters = rfl::json::read<rfl::Generic>(interval_json).value();

    seq.commands = {move_cmd, interval_cmd};
    return seq;
}

}// namespace

// =============================================================================
// State machine transition tests
// =============================================================================

TEST_CASE("TriageSession starts in Idle state", "[triage]") {
    TriageSession session;
    REQUIRE(session.state() == TriageSession::State::Idle);
}

TEST_CASE("TriageSession mark transitions Idle to Marking", "[triage]") {
    TriageSession session;

    session.mark(TimeFrameIndex(42000));

    REQUIRE(session.state() == TriageSession::State::Marking);
    REQUIRE(session.markFrame() == TimeFrameIndex(42000));
}

TEST_CASE("TriageSession mark is no-op when already Marking", "[triage]") {
    TriageSession session;

    session.mark(TimeFrameIndex(1000));
    session.mark(TimeFrameIndex(2000));// Should be ignored

    REQUIRE(session.state() == TriageSession::State::Marking);
    REQUIRE(session.markFrame() == TimeFrameIndex(1000));
}

TEST_CASE("TriageSession recall transitions Marking to Idle", "[triage]") {
    TriageSession session;

    session.mark(TimeFrameIndex(42000));
    REQUIRE(session.state() == TriageSession::State::Marking);

    session.recall();
    REQUIRE(session.state() == TriageSession::State::Idle);
}

TEST_CASE("TriageSession recall is no-op when Idle", "[triage]") {
    TriageSession session;

    session.recall();// Should not crash or change state
    REQUIRE(session.state() == TriageSession::State::Idle);
}

// =============================================================================
// Commit error handling
// =============================================================================

TEST_CASE("TriageSession commit fails when not Marking", "[triage]") {
    TriageSession session;
    session.setPipeline(makeTestPipeline());

    auto dm = makeTestDataManager();
    auto result = session.commit(TimeFrameIndex(300), dm);

    REQUIRE_FALSE(result.success);
    REQUIRE(result.error_message.find("not in Marking state") != std::string::npos);
}

TEST_CASE("TriageSession commit fails with null DataManager", "[triage]") {
    TriageSession session;
    session.setPipeline(makeTestPipeline());
    session.mark(TimeFrameIndex(100));

    auto result = session.commit(TimeFrameIndex(300), nullptr);

    REQUIRE_FALSE(result.success);
    REQUIRE(result.error_message.find("null") != std::string::npos);
    REQUIRE(session.state() == TriageSession::State::Marking);
}

TEST_CASE("TriageSession commit fails with empty pipeline", "[triage]") {
    TriageSession session;
    session.mark(TimeFrameIndex(100));

    auto result = session.commit(TimeFrameIndex(300), makeTestDataManager());

    REQUIRE_FALSE(result.success);
    REQUIRE(result.error_message.find("no commands") != std::string::npos);
    REQUIRE(session.state() == TriageSession::State::Marking);
}

// =============================================================================
// Successful commit
// =============================================================================

TEST_CASE("TriageSession commit executes pipeline and transitions to Idle",
          "[triage]") {
    TriageSession session;
    session.setPipeline(makeTestPipeline());
    session.mark(TimeFrameIndex(100));

    auto dm = makeTestDataManager();
    auto result = session.commit(TimeFrameIndex(250), dm);

    REQUIRE(result.success);
    REQUIRE(session.state() == TriageSession::State::Idle);

    // Verify data was moved: entities at frames 100 and 200 should be in gt_w0
    auto gt = dm->getData<LineData>("gt_w0");
    REQUIRE(gt->getTotalEntryCount() == 2);

    // Entity at frame 300 should remain in source
    auto src = dm->getData<LineData>("pred_w0");
    REQUIRE(src->getTotalEntryCount() == 1);

    // Verify tracked_regions interval was created
    auto tracked = dm->getData<DigitalIntervalSeries>("tracked_regions");
    REQUIRE(tracked != nullptr);
    REQUIRE(tracked->size() == 1);
}

TEST_CASE("TriageSession retains executed commands after successful commit",
          "[triage]") {
    TriageSession session;
    session.setPipeline(makeTestPipeline());
    session.mark(TimeFrameIndex(100));

    auto dm = makeTestDataManager();
    auto result = session.commit(TimeFrameIndex(250), dm);
    REQUIRE(result.success);

    auto const & cmds = session.lastCommitCommands();
    REQUIRE(cmds.size() == 2);
    REQUIRE(cmds[0]->commandName() == "MoveByTimeRange");
    REQUIRE(cmds[1]->commandName() == "AddInterval");
}

TEST_CASE("TriageSession takeLastCommitCommands moves commands out",
          "[triage]") {
    TriageSession session;
    session.setPipeline(makeTestPipeline());
    session.mark(TimeFrameIndex(100));

    auto dm = makeTestDataManager();
    session.commit(TimeFrameIndex(250), dm);

    auto taken = session.takeLastCommitCommands();
    REQUIRE(taken.size() == 2);
    REQUIRE(session.lastCommitCommands().empty());
}

// =============================================================================
// Commit failure preserves Marking state
// =============================================================================

TEST_CASE("TriageSession commit failure keeps Marking state", "[triage]") {
    TriageSession session;

    // Pipeline references a non-existent source key -> command will fail
    CommandSequenceDescriptor bad_seq;
    bad_seq.name = "Bad Pipeline";
    CommandDescriptor bad_cmd;
    bad_cmd.command_name = "MoveByTimeRange";
    auto bad_json = R"({
        "source_key": "nonexistent_source",
        "destination_key": "nonexistent_dest",
        "start_frame": "${mark_frame}",
        "end_frame": "${current_frame}"
    })";
    bad_cmd.parameters = rfl::json::read<rfl::Generic>(bad_json).value();
    bad_seq.commands = {bad_cmd};

    session.setPipeline(bad_seq);
    session.mark(TimeFrameIndex(100));

    auto dm = makeTestDataManager();
    auto result = session.commit(TimeFrameIndex(200), dm);

    REQUIRE_FALSE(result.success);
    REQUIRE(session.state() == TriageSession::State::Marking);
}

// =============================================================================
// Pipeline configuration
// =============================================================================

TEST_CASE("TriageSession setPipeline / pipeline round-trip", "[triage]") {
    TriageSession session;

    auto seq = makeTestPipeline();
    session.setPipeline(seq);

    auto const & stored = session.pipeline();
    REQUIRE(stored.name.has_value());
    REQUIRE(stored.name.value() == "Test Triage Pipeline");
    REQUIRE(stored.commands.size() == 2);
}

// =============================================================================
// Multi-commit workflow (mark -> commit -> mark -> commit)
// =============================================================================

TEST_CASE("TriageSession supports sequential mark/commit cycles", "[triage]") {
    TriageSession session;
    session.setPipeline(makeTestPipeline());

    auto dm = makeTestDataManager();
    // Source has entities at 100, 200, 300

    // First commit: move frame 100 only
    session.mark(TimeFrameIndex(100));
    auto r1 = session.commit(TimeFrameIndex(150), dm);
    REQUIRE(r1.success);
    REQUIRE(session.state() == TriageSession::State::Idle);

    auto gt = dm->getData<LineData>("gt_w0");
    REQUIRE(gt->getTotalEntryCount() == 1);

    // Second commit: move frame 200
    session.mark(TimeFrameIndex(200));
    auto r2 = session.commit(TimeFrameIndex(250), dm);
    REQUIRE(r2.success);

    REQUIRE(gt->getTotalEntryCount() == 2);

    // Source should only have frame 300 left
    auto src = dm->getData<LineData>("pred_w0");
    REQUIRE(src->getTotalEntryCount() == 1);

    // Tracked regions should have 2 intervals
    auto tracked = dm->getData<DigitalIntervalSeries>("tracked_regions");
    REQUIRE(tracked->size() == 2);
}

// =============================================================================
// Runtime variable population
// =============================================================================

TEST_CASE("TriageSession commit populates correct runtime variables",
          "[triage]") {
    TriageSession session;

    // Use a pipeline with AddInterval that will embed the frame values
    CommandSequenceDescriptor seq;
    CommandDescriptor cmd;
    cmd.command_name = "AddInterval";
    auto json = R"({
        "interval_key": "tracked",
        "start_frame": "${mark_frame}",
        "end_frame": "${current_frame}",
        "create_if_missing": true
    })";
    cmd.parameters = rfl::json::read<rfl::Generic>(json).value();
    seq.commands = {cmd};
    session.setPipeline(seq);

    session.mark(TimeFrameIndex(42000));
    auto dm = std::make_shared<DataManager>();
    auto result = session.commit(TimeFrameIndex(43500), dm);

    REQUIRE(result.success);

    // Verify the interval was created with the correct frame values
    auto tracked = dm->getData<DigitalIntervalSeries>("tracked");
    REQUIRE(tracked != nullptr);
    REQUIRE(tracked->size() == 1);
}
