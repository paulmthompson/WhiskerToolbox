/**
 * @file register_transformsv2_commands.cpp
 * @brief Registers TransformsV2-owned commands with the CommandRegistry singleton
 */

#include "TransformsV2/register_transformsv2_commands.hpp"

#include "Commands/Core/CommandRegistry.hpp"
#include "TransformsV2/Commands/RunTransformsV2PipelineAtTime.hpp"
#include "TransformsV2/Commands/RunTransformsV2PipelineAtTimeUIHints.hpp"

namespace WhiskerToolbox::Transforms::V2 {

void register_transformsv2_commands() {
    auto & reg = commands::CommandRegistry::instance();

    if (reg.isKnown("RunTransformsV2PipelineAtTime")) {
        return;
    }

    commands::registerTypedCommand<commands::RunTransformsV2PipelineAtTime,
                                 commands::RunTransformsV2PipelineAtTimeParams>(
            reg,
            "RunTransformsV2PipelineAtTime",
            {.name = "RunTransformsV2PipelineAtTime",
             .description =
                     "Run a saved TransformsV2 pipeline on a single frame (stub)",
             .category = "transforms",
             .supports_undo = false,
             .supported_data_types = {"LineData", "MaskData", "PointData"}});
}

}// namespace WhiskerToolbox::Transforms::V2
