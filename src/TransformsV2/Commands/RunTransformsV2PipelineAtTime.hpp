/**
 * @file RunTransformsV2PipelineAtTime.hpp
 * @brief Command to run a saved TransformsV2 pipeline on a single frame (stub)
 */

#ifndef RUN_TRANSFORMS_V2_PIPELINE_AT_TIME_COMMAND_HPP
#define RUN_TRANSFORMS_V2_PIPELINE_AT_TIME_COMMAND_HPP

#include "Commands/Core/CommandResult.hpp"
#include "Commands/Core/ICommand.hpp"

#include <cstdint>
#include <string>

#include <rfl/json.hpp>

namespace commands {

/**
 * @brief Parameters for `RunTransformsV2PipelineAtTime`
 */
struct RunTransformsV2PipelineAtTimeParams {
    std::string input_key;
    std::string output_key;
    /// Frame index; sequence JSON may use "${current_frame}" / "${mark_frame}" templates
    int64_t time = 0;
    std::string pipeline_path;
};

/**
 * @brief Runs a saved TransformsV2 pipeline on one frame (execution stub)
 */
class RunTransformsV2PipelineAtTime : public ICommand {
public:
    /**
     * @brief Construct from deserialized parameters
     */
    explicit RunTransformsV2PipelineAtTime(RunTransformsV2PipelineAtTimeParams p);

    [[nodiscard]] std::string commandName() const override;
    [[nodiscard]] std::string toJson() const override;

    /// @pre `ctx.data_manager` is non-null
    /// @post No DataManager mutation (stub implementation)
    CommandResult execute(CommandContext const & ctx) override;

private:
    RunTransformsV2PipelineAtTimeParams _params;
};

}// namespace commands

#endif// RUN_TRANSFORMS_V2_PIPELINE_AT_TIME_COMMAND_HPP
