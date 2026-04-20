/**
 * @file KeymapCommandBridge.cpp
 * @brief Bridges KeymapSystem hotkeys to Commands::executeSequence via EditorRegistry
 */

#include "KeymapSystem/KeymapCommandBridge.hpp"

#include "Commands/Core/CommandContext.hpp"
#include "Commands/Core/SequenceExecution.hpp"

#include "DataManager/DataManager.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "TimeController/TimeController.hpp"

#include <spdlog/spdlog.h>

#include <string>

namespace KeymapSystem {

commands::SequenceResult executeCommandSequenceFromRegistry(
        std::shared_ptr<DataManager> data_manager,
        EditorRegistry * registry,
        commands::CommandSequenceDescriptor const & seq) {

    commands::SequenceResult failure{};
    failure.result =
            commands::CommandResult::error("executeCommandSequenceFromRegistry: DataManager is null");

    if (!data_manager) {
        spdlog::warn("[KeymapCommandBridge] {}", failure.result.error_message);
        return failure;
    }

    commands::CommandContext ctx;
    ctx.data_manager = std::move(data_manager);
    ctx.time_controller = registry != nullptr ? registry->timeController() : nullptr;

    if (ctx.time_controller != nullptr) {
        ctx.runtime_variables["current_frame"] =
                std::to_string(ctx.time_controller->currentTimeIndex().getValue());
        ctx.runtime_variables["current_time_key"] =
                ctx.time_controller->activeTimeKey().str();
    }

    return commands::executeSequence(seq, ctx);
}

}// namespace KeymapSystem
