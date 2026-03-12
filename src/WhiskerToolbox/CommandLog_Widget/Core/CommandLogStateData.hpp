/**
 * @file CommandLogStateData.hpp
 * @brief Serializable state data structure for CommandLog widget
 */

#ifndef COMMAND_LOG_STATE_DATA_HPP
#define COMMAND_LOG_STATE_DATA_HPP

#include <rfl.hpp>

#include <string>

/**
 * @brief Serializable data structure for CommandLogState
 *
 * Minimal state — the Command Log widget has no persistent configuration
 * beyond its instance identity.
 */
struct CommandLogStateData {
    std::string instance_id;
    std::string display_name = "Command Log";
};

#endif// COMMAND_LOG_STATE_DATA_HPP
