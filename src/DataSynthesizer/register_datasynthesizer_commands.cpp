/**
 * @file register_datasynthesizer_commands.cpp
 * @brief Registers DataSynthesizer-owned commands with the CommandRegistry singleton
 */

#include "DataSynthesizer/register_datasynthesizer_commands.hpp"

#include "Commands/Core/CommandRegistry.hpp"
#include "DataSynthesizer/Commands/SynthesizeData.hpp"

namespace Neuralyzer::DataSynthesizer {

void register_datasynthesizer_commands() {
    auto & reg = commands::CommandRegistry::instance();

    if (reg.isKnown("SynthesizeData")) {
        return;
    }

    commands::registerTypedCommand<commands::SynthesizeData, commands::SynthesizeDataParams>(
            reg,
            "SynthesizeData",
            {.name = "SynthesizeData",
             .description = "Generate synthetic data using a registered generator and store it in DataManager",
             .category = "data_generation",
             .supports_undo = false,
             .supported_data_types = {"AnalogTimeSeries"}});
}

}// namespace Neuralyzer::DataSynthesizer
