/**
 * @file SynthesizeData.cpp
 * @brief Implementation of the SynthesizeData command
 */

#include "SynthesizeData.hpp"

#include "Core/CommandContext.hpp"

#include "DataManager/DataManager.hpp"
#include "DataSynthesizer/GeneratorRegistry.hpp"

#include "TimeFrame/StrongTimeTypes.hpp"

#include <rfl/json.hpp>

namespace commands {

SynthesizeData::SynthesizeData(SynthesizeDataParams p)
    : _params(std::move(p)) {}

std::string SynthesizeData::commandName() const { return "SynthesizeData"; }

std::string SynthesizeData::toJson() const { return rfl::json::write(_params); }

CommandResult SynthesizeData::execute(CommandContext const & ctx) {
    auto & registry = WhiskerToolbox::DataSynthesizer::GeneratorRegistry::instance();

    if (!registry.hasGenerator(_params.generator_name)) {
        return CommandResult::error(
                "Unknown generator: '" + _params.generator_name + "'");
    }

    auto const params_json = rfl::json::write(_params.parameters);

    auto result = registry.generate(_params.generator_name, params_json);
    if (!result.has_value()) {
        return CommandResult::error(
                "Generator '" + _params.generator_name + "' failed to produce data");
    }

    ctx.data_manager->setData(
            _params.output_key,
            std::move(*result),
            TimeKey(_params.time_key.value_or("time")));

    return CommandResult::ok({_params.output_key});
}

}// namespace commands
