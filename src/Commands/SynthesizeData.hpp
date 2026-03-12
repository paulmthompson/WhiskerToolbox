/**
 * @file SynthesizeData.hpp
 * @brief Command to generate data using the DataSynthesizer registry and store it in DataManager
 */

#ifndef SYNTHESIZE_DATA_COMMAND_HPP
#define SYNTHESIZE_DATA_COMMAND_HPP

#include "Core/CommandResult.hpp"
#include "Core/ICommand.hpp"

#include <optional>
#include <string>

#include <rfl.hpp>
#include <rfl/json.hpp>

namespace commands {

struct SynthesizeDataParams {
    std::string output_key;
    std::string generator_name;
    std::string output_type;
    rfl::Generic parameters;
    std::optional<std::string> time_key;

    /// "create_new" (default): create a new TimeFrame; fails if key already exists.
    /// "use_existing": attach to an existing TimeFrame; fails if it doesn't exist or is too small.
    /// "overwrite": create/replace a TimeFrame unconditionally.
    std::optional<std::string> time_frame_mode;
};

class SynthesizeData : public ICommand {
public:
    explicit SynthesizeData(SynthesizeDataParams p);

    [[nodiscard]] std::string commandName() const override;
    [[nodiscard]] std::string toJson() const override;

    CommandResult execute(CommandContext const & ctx) override;

private:
    SynthesizeDataParams _params;
};

}// namespace commands

#endif// SYNTHESIZE_DATA_COMMAND_HPP
