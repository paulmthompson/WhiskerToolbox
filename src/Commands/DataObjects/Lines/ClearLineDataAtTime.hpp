/**
 * @file ClearLineDataAtTime.hpp
 * @brief Command to remove all LineData entries at a single time index
 */

#ifndef CLEAR_LINE_DATA_AT_TIME_COMMAND_HPP
#define CLEAR_LINE_DATA_AT_TIME_COMMAND_HPP

#include "Core/CommandResult.hpp"
#include "Core/ICommand.hpp"

#include <cstdint>
#include <string>

#include <rfl/json.hpp>

namespace commands {

/**
 * @brief Parameters for `ClearLineDataAtTime` (JSON fields `data_key`, `time`).
 */
struct ClearLineDataAtTimeParams {
    std::string data_key;
    int64_t time = 0;
};

/**
 * @brief Removes all `LineData` entries at one frame via `LineData::clearAtTime`.
 */
class ClearLineDataAtTime : public ICommand {
public:
    /**
     * @brief Construct from deserialized parameters.
     */
    explicit ClearLineDataAtTime(ClearLineDataAtTimeParams p);

    [[nodiscard]] std::string commandName() const override;
    [[nodiscard]] std::string toJson() const override;

    /// @pre `ctx.data_manager` is non-null and holds `LineData` at `data_key`.
    /// @post All entries at the resolved frame in that `LineData` are removed.
    CommandResult execute(CommandContext const & ctx) override;

private:
    ClearLineDataAtTimeParams _params;
};

}// namespace commands

#endif// CLEAR_LINE_DATA_AT_TIME_COMMAND_HPP
