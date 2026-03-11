/**
 * @file CommandRecorder.hpp
 * @brief Lightweight recorder that captures executed CommandDescriptors for replay and testing
 */

#ifndef COMMAND_RECORDER_HPP
#define COMMAND_RECORDER_HPP

#include "CommandDescriptor.hpp"

#include <string>
#include <vector>

namespace commands {

/// @brief Records CommandDescriptors during sequence execution for replay, golden trace
/// tests, and eventual integration into the Action Journal (Phase 4).
///
/// This is intentionally minimal: it appends descriptors, returns the trace, and can
/// produce a CommandSequenceDescriptor suitable for JSON serialization and replay.
class CommandRecorder {
public:
    /// @brief Append a command descriptor to the trace
    void record(CommandDescriptor desc);

    /// @brief Access the recorded trace
    [[nodiscard]] auto const & trace() const { return _trace; }

    /// @brief Produce a CommandSequenceDescriptor from the recorded trace
    /// @param name Optional name for the generated sequence
    [[nodiscard]] CommandSequenceDescriptor toSequence(std::string name = "") const;

    /// @brief Reset the recorder, clearing all recorded descriptors
    void clear();

    /// @brief Number of recorded descriptors
    [[nodiscard]] size_t size() const { return _trace.size(); }

    /// @brief Whether the trace is empty
    [[nodiscard]] bool empty() const { return _trace.empty(); }

private:
    std::vector<CommandDescriptor> _trace;
};

}// namespace commands

#endif// COMMAND_RECORDER_HPP
