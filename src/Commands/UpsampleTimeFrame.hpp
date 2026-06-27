/**
 * @file UpsampleTimeFrame.hpp
 * @brief Command to create an upsampled TimeFrame by linear clock interpolation
 */

#ifndef UPSAMPLE_TIME_FRAME_COMMAND_HPP
#define UPSAMPLE_TIME_FRAME_COMMAND_HPP

#include "Core/CommandResult.hpp"
#include "Core/ICommand.hpp"

#include <cstdint>
#include <string>

#include <rfl/json.hpp>

namespace commands {

/**
 * @brief Parameters for `UpsampleTimeFrame`
 */
struct UpsampleTimeFrameParams {
    std::string source_time_key;
    std::string output_time_key;
    int64_t upsampling_factor = 2;
    bool overwrite = false;
};

/**
 * @brief Creates a new TimeFrame by upsampling an existing one
 */
class UpsampleTimeFrame : public ICommand {
public:
    explicit UpsampleTimeFrame(UpsampleTimeFrameParams p);

    [[nodiscard]] std::string commandName() const override;
    [[nodiscard]] std::string toJson() const override;

    /// @pre `ctx.data_manager` is non-null; `source_time_key` and `output_time_key` are non-empty
    /// @pre `source_time_key` exists in DataManager; `upsampling_factor` is > 0
    /// @post On success, `output_time_key` is registered with the upsampled TimeFrame
    CommandResult execute(CommandContext const & ctx) override;

private:
    UpsampleTimeFrameParams _params;
};

}// namespace commands

#endif// UPSAMPLE_TIME_FRAME_COMMAND_HPP
