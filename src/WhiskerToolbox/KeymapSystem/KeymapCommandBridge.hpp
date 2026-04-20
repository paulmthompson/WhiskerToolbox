/**
 * @file KeymapCommandBridge.hpp
 * @brief Builds CommandContext from EditorRegistry and runs executeSequence()
 */

#ifndef KEYMAP_SYSTEM_KEYMAP_COMMAND_BRIDGE_HPP
#define KEYMAP_SYSTEM_KEYMAP_COMMAND_BRIDGE_HPP

#include "Commands/Core/SequenceExecution.hpp"

#include <memory>

class DataManager;
class EditorRegistry;

namespace KeymapSystem {

/// @brief Run a command sequence using @p data_manager and time state from @p registry.
///
/// Sets runtime_variables @c current_frame (from TimeController index) and
/// @c current_time_key (from TimeController active key string).
///
/// @param data_manager Application DataManager (non-null required for success).
/// @param registry Used for EditorRegistry::timeController(); may be null if sequences need
///        only DataManager (navigation commands that use TimeController will fail otherwise).
///
/// @return Sequence execution outcome; fails if @p data_manager is null.
[[nodiscard]] commands::SequenceResult executeCommandSequenceFromRegistry(
        std::shared_ptr<DataManager> data_manager,
        EditorRegistry * registry,
        commands::CommandSequenceDescriptor const & seq);

}// namespace KeymapSystem

#endif// KEYMAP_SYSTEM_KEYMAP_COMMAND_BRIDGE_HPP
