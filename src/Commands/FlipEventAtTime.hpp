/**
 * @file FlipEventAtTime.hpp
 * @brief Command to toggle DigitalIntervalSeries coverage at a single time index
 */

#ifndef FLIP_EVENT_AT_TIME_COMMAND_HPP
#define FLIP_EVENT_AT_TIME_COMMAND_HPP

#include "Core/CommandResult.hpp"
#include "Core/ICommand.hpp"

#include <cstdint>
#include <string>

#include <rfl/json.hpp>

namespace commands {

/// @brief Parameters for `FlipEventAtTime` (JSON field `data_key`, `time`).
struct FlipEventAtTimeParams {
    std::string data_key;
    int64_t time = 0;
};

/**
 * @brief Toggles interval membership at `time` using `hasIntervalAtTime` then
 *        `setEventAtTime` with the negated value.
 */
class FlipEventAtTime : public ICommand {
public:
    /// @brief Construct from deserialized parameters.
    explicit FlipEventAtTime(FlipEventAtTimeParams p);

    [[nodiscard]] std::string commandName() const override;
    [[nodiscard]] std::string toJson() const override;

    /// @pre `ctx.data_manager` is non-null and holds a `DigitalIntervalSeries` at `data_key`.
    CommandResult execute(CommandContext const & ctx) override;

private:
    FlipEventAtTimeParams _params;
};

}// namespace commands

#endif// FLIP_EVENT_AT_TIME_COMMAND_HPP
