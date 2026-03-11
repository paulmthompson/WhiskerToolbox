/**
 * @file CommandRecorder.cpp
 * @brief Implementation of CommandRecorder for capturing command execution traces
 */

#include "CommandRecorder.hpp"

#include <utility>

namespace commands {

void CommandRecorder::record(CommandDescriptor desc) {
    _trace.push_back(std::move(desc));
}

CommandSequenceDescriptor CommandRecorder::toSequence(std::string name) const {
    CommandSequenceDescriptor seq;
    if (!name.empty()) {
        seq.name = std::move(name);
    }
    seq.commands = _trace;
    return seq;
}

void CommandRecorder::clear() {
    _trace.clear();
}

}// namespace commands
