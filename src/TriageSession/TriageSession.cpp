/**
 * @file TriageSession.cpp
 * @brief Implementation of the Mark/Commit/Recall triage state machine
 */

#include "TriageSession.hpp"

#include "Commands/Core/CommandContext.hpp"
#include "Commands/Core/SequenceExecution.hpp"

#include <string>
#include <utility>

namespace triage {

TriageSession::TriageSession() = default;

// -- Configuration ------------------------------------------------------------

void TriageSession::setPipeline(commands::CommandSequenceDescriptor pipeline) {
    _pipeline = std::move(pipeline);
}

commands::CommandSequenceDescriptor const & TriageSession::pipeline() const {
    return _pipeline;
}

// -- State machine ------------------------------------------------------------

void TriageSession::mark(TimeFrameIndex frame) {
    if (_state == State::Marking) {
        return;
    }
    _state = State::Marking;
    _mark_frame = frame;
    _last_commit_commands.clear();
}

void TriageSession::recall() {
    if (_state == State::Idle) {
        return;
    }
    _state = State::Idle;
}

commands::CommandResult TriageSession::commit(TimeFrameIndex current_frame,
                                              std::shared_ptr<DataManager> dm,
                                              commands::CommandRecorder * recorder) {
    if (_state != State::Marking) {
        return commands::CommandResult::error("Cannot commit: session is not in Marking state");
    }

    if (!dm) {
        return commands::CommandResult::error("Cannot commit: DataManager is null");
    }

    if (_pipeline.commands.empty()) {
        return commands::CommandResult::error("Cannot commit: pipeline has no commands");
    }

    commands::CommandContext ctx;
    ctx.data_manager = std::move(dm);
    ctx.recorder = recorder;
    ctx.runtime_variables["mark_frame"] = std::to_string(_mark_frame.getValue());
    ctx.runtime_variables["current_frame"] = std::to_string(current_frame.getValue());

    auto seq_result = commands::executeSequence(_pipeline, ctx);

    if (seq_result.result.success) {
        _state = State::Idle;
        _last_commit_commands = std::move(seq_result.executed_commands);
    }

    return seq_result.result;
}

// -- Query --------------------------------------------------------------------

TriageSession::State TriageSession::state() const {
    return _state;
}

TimeFrameIndex TriageSession::markFrame() const {
    return _mark_frame;
}

std::vector<std::unique_ptr<commands::ICommand>> const &
TriageSession::lastCommitCommands() const {
    return _last_commit_commands;
}

std::vector<std::unique_ptr<commands::ICommand>>
TriageSession::takeLastCommitCommands() {
    return std::move(_last_commit_commands);
}

}// namespace triage
