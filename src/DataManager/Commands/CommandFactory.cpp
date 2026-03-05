/**
 * @file CommandFactory.cpp
 * @brief Implementation of the command factory function
 */

#include "CommandFactory.hpp"

#include "AddInterval.hpp"
#include "CopyByTimeRange.hpp"
#include "ForEachKey.hpp"
#include "ICommand.hpp"
#include "MoveByTimeRange.hpp"

#include <rfl/json.hpp>

namespace commands {

bool isKnownCommandName(std::string const & name) {
    return name == "MoveByTimeRange" || name == "CopyByTimeRange" ||
           name == "AddInterval" || name == "ForEachKey";
}

std::unique_ptr<ICommand> createCommand(
        std::string const & name,
        rfl::Generic const & params) {

    auto const json = rfl::json::write(params);

    if (name == "MoveByTimeRange") {
        auto p = rfl::json::read<MoveByTimeRangeParams>(json);
        if (!p) return nullptr;
        return std::make_unique<MoveByTimeRange>(std::move(*p));
    }

    if (name == "CopyByTimeRange") {
        auto p = rfl::json::read<CopyByTimeRangeParams>(json);
        if (!p) return nullptr;
        return std::make_unique<CopyByTimeRange>(std::move(*p));
    }

    if (name == "AddInterval") {
        auto p = rfl::json::read<AddIntervalParams>(json);
        if (!p) return nullptr;
        return std::make_unique<AddInterval>(std::move(*p));
    }

    if (name == "ForEachKey") {
        auto p = rfl::json::read<ForEachKeyParams>(json);
        if (!p) return nullptr;
        return std::make_unique<ForEachKey>(std::move(*p));
    }

    return nullptr;
}

}// namespace commands
