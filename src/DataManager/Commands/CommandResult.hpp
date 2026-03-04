/**
 * @file CommandResult.hpp
 * @brief Result type returned by command execution
 */

#ifndef COMMAND_RESULT_HPP
#define COMMAND_RESULT_HPP

#include <string>
#include <utility>
#include <vector>

namespace commands {

/// @brief Result of executing a command
struct CommandResult {
    bool success = false;
    std::string error_message;

    /// Keys of data objects that were created or modified
    std::vector<std::string> affected_keys;

    static CommandResult ok(std::vector<std::string> keys = {}) {
        return {true, {}, std::move(keys)};
    }

    static CommandResult error(std::string msg) {
        return {false, std::move(msg), {}};
    }
};

}// namespace commands

#endif// COMMAND_RESULT_HPP
