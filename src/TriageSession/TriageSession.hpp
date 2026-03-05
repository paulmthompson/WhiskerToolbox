/**
 * @file TriageSession.hpp
 * @brief Mark/Commit/Recall state machine for triage workflows
 *
 * TriageSession manages a simple Idle/Marking state machine. On commit,
 * it populates runtime variables (mark_frame, current_frame) and delegates
 * to executeSequence() from the Command Architecture.
 */

#ifndef TRIAGE_SESSION_HPP
#define TRIAGE_SESSION_HPP

#include "DataManager/Commands/CommandDescriptor.hpp"
#include "DataManager/Commands/CommandResult.hpp"
#include "DataManager/Commands/ICommand.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

#include <memory>
#include <vector>

class DataManager;

namespace triage {

/// @brief Mark/Commit/Recall state machine consuming command sequences.
///
/// The triage session is a consumer of the command architecture, not a bespoke
/// system. It manages a simple state machine and, on commit, executes a
/// user-configured CommandSequenceDescriptor.
///
/// State transitions:
///   IDLE  --[mark]--> MARKING --[commit]--> IDLE
///                         |--[recall]--> IDLE
class TriageSession {
public:
    enum class State { Idle,
                       Marking };

    TriageSession();

    // -- Configuration --------------------------------------------------------

    /// @brief Set the command pipeline to execute on commit.
    void setPipeline(commands::CommandSequenceDescriptor pipeline);

    /// @brief Access the current pipeline.
    [[nodiscard]] commands::CommandSequenceDescriptor const & pipeline() const;

    // -- State machine --------------------------------------------------------

    /// @brief Begin marking from the given frame. Transitions Idle -> Marking.
    ///        No-op if already Marking.
    void mark(TimeFrameIndex frame);

    /// @brief Abort the current marking and reset to Idle.
    ///        The caller is responsible for jumping back to the mark frame.
    ///        No-op if already Idle.
    void recall();

    /// @brief Execute the pipeline for [mark_frame, current_frame] and
    ///        transition to Idle on success.
    /// @param current_frame The end frame of the triage range
    /// @param dm The DataManager to operate on
    /// @return CommandResult indicating success or failure.
    ///         On failure, state remains Marking so the user can retry or recall.
    commands::CommandResult commit(TimeFrameIndex current_frame,
                                   std::shared_ptr<DataManager> dm);

    // -- Query ----------------------------------------------------------------

    /// @brief Current state of the session.
    [[nodiscard]] State state() const;

    /// @brief The frame where marking started. Only meaningful when state == Marking.
    [[nodiscard]] TimeFrameIndex markFrame() const;

    /// @brief Commands from the last successful commit (retained for undo support).
    [[nodiscard]] std::vector<std::unique_ptr<commands::ICommand>> const &
    lastCommitCommands() const;

    /// @brief Take ownership of the last commit's commands (moves them out).
    std::vector<std::unique_ptr<commands::ICommand>> takeLastCommitCommands();

private:
    commands::CommandSequenceDescriptor _pipeline;
    State _state = State::Idle;
    TimeFrameIndex _mark_frame{0};

    /// Commands from the most recent successful commit, retained for undo.
    std::vector<std::unique_ptr<commands::ICommand>> _last_commit_commands;
};

}// namespace triage

#endif// TRIAGE_SESSION_HPP
