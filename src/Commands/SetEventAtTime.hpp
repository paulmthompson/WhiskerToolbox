/**
 * @file SetEventAtTime.hpp
 * @brief Command to set or clear a DigitalIntervalSeries sample at a single time index
 */

#ifndef SET_EVENT_AT_TIME_COMMAND_HPP
#define SET_EVENT_AT_TIME_COMMAND_HPP

#include "Core/CommandResult.hpp"
#include "Core/ICommand.hpp"

#include <cstdint>
#include <string>

#include <rfl/json.hpp>

namespace commands {

/// @brief Parameters for `SetEventAtTime` (JSON field `data_key`, `time`, `event`).
struct SetEventAtTimeParams {
    std::string data_key;
    int64_t time = 0;
    bool event = false;
};

/**
 * @brief Sets `DigitalIntervalSeries::setEventAtTime` for one frame (`event` true adds a
 *        one-frame interval; false removes coverage at that time).
 */
class SetEventAtTime : public ICommand {
public:
    /// @brief Construct from deserialized parameters.
    explicit SetEventAtTime(SetEventAtTimeParams p);

    [[nodiscard]] std::string commandName() const override;
    [[nodiscard]] std::string toJson() const override;

    /// @pre `ctx.data_manager` is non-null and holds a `DigitalIntervalSeries` at `data_key`.
    CommandResult execute(CommandContext const & ctx) override;

private:
    SetEventAtTimeParams _params;
};

}// namespace commands

#endif// SET_EVENT_AT_TIME_COMMAND_HPP
