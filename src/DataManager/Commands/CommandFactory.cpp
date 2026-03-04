/**
 * @file CommandFactory.cpp
 * @brief Implementation of the command factory function
 */

#include "CommandFactory.hpp"

#include "ICommand.hpp"

#include <rfl/json.hpp>

namespace commands {

std::unique_ptr<ICommand> createCommand(
        std::string const & name,
        rfl::Generic const & /*params*/) {
    // Future commands will be added here as if-branches, e.g.:
    //
    // if (name == "MoveByTimeRange") {
    //     auto const json = rfl::json::write(params);
    //     auto p = rfl::json::read<MoveByTimeRangeParams>(json);
    //     if (!p) return nullptr;
    //     return std::make_unique<MoveByTimeRange>(std::move(*p));
    // }

    // Suppress unused variable warning until concrete commands are added
    (void) name;

    return nullptr;
}

}// namespace commands
