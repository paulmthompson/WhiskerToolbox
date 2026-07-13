/**
 * @file CommandRecorder.hpp
 * @brief Lightweight recorder that captures executed CommandDescriptors for replay and testing.
 */

#ifndef COMMAND_RECORDER_HPP
#define COMMAND_RECORDER_HPP

#include "CommandDescriptor.hpp"

#include <string>
#include <vector>

namespace commands {

/**
 * @brief Records CommandDescriptors during sequence execution for replay, golden trace
 *        tests, and eventual integration into the Action Journal (Phase 4).
 *
 * This is intentionally minimal: it appends descriptors, returns the trace, and can
 * produce a CommandSequenceDescriptor suitable for JSON serialization and replay.
 */
class CommandRecorder {
public:
    /**
     * @brief Append a command descriptor to the trace.
     *
     * @param desc Descriptor to record (moved into internal storage).
     */
    void record(CommandDescriptor desc);

    /**
     * @brief Access the recorded trace.
     *
     * @return Const reference to the ordered list of recorded descriptors.
     */
    [[nodiscard]] auto const & trace() const { return _trace; }

    /**
     * @brief Produce a CommandSequenceDescriptor from the recorded trace.
     *
     * @param name Optional name for the generated sequence.
     * @return A sequence descriptor containing the recorded commands.
     */
    [[nodiscard]] CommandSequenceDescriptor toSequence(std::string name = "") const;

    /**
     * @brief Reset the recorder, clearing all recorded descriptors.
     */
    void clear();

    /**
     * @brief Return the number of recorded descriptors.
     *
     * @return Count of descriptors in the trace.
     */
    [[nodiscard]] size_t size() const { return _trace.size(); }

    /**
     * @brief Report whether the trace is empty.
     *
     * @return True if no descriptors have been recorded.
     */
    [[nodiscard]] bool empty() const { return _trace.empty(); }

private:
    std::vector<CommandDescriptor> _trace;
};

}// namespace commands

#endif// COMMAND_RECORDER_HPP
