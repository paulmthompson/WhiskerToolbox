/**
 * @file RunTransformsV2Pipeline.hpp
 * @brief Command to run a saved TransformsV2 pipeline on an entire data object
 */

#ifndef RUN_TRANSFORMS_V2_PIPELINE_COMMAND_HPP
#define RUN_TRANSFORMS_V2_PIPELINE_COMMAND_HPP

#include "Commands/Core/CommandResult.hpp"
#include "Commands/Core/ICommand.hpp"

#include <string>

#include <rfl/json.hpp>

namespace commands {

/**
 * @brief Parameters for `RunTransformsV2Pipeline`
 */
struct RunTransformsV2PipelineParams {
    std::string input_key;
    std::string output_key;
    std::string pipeline_path;
    /// When empty, inherit TimeKey from input_key
    std::string output_time_key;
};

/**
 * @brief Runs a saved TransformsV2 pipeline on an entire DataManager data object
 */
class RunTransformsV2Pipeline : public ICommand {
public:
    /**
     * @brief Construct from deserialized parameters
     */
    explicit RunTransformsV2Pipeline(RunTransformsV2PipelineParams p);

    [[nodiscard]] std::string commandName() const override;
    [[nodiscard]] std::string toJson() const override;

    /// @pre `ctx.data_manager` is non-null; `input_key` and `output_key` are non-empty
    /// @pre `input_key` exists in the DataManager
    /// @post On success, `output_key` is created or replaced with full pipeline output
    CommandResult execute(CommandContext const & ctx) override;

private:
    RunTransformsV2PipelineParams _params;
};

}// namespace commands

#endif// RUN_TRANSFORMS_V2_PIPELINE_COMMAND_HPP
