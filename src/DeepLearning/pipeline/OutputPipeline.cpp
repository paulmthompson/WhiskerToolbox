/**
 * @file OutputPipeline.cpp
 * @brief Implementation of output pipeline metadata helpers.
 */

#include "OutputPipeline.hpp"

#include <algorithm>
#include <ranges>
#include <type_traits>
#include <utility>

namespace dl {

namespace {

[[nodiscard]] bool acceptsRank(PipelineStepDescriptor const & desc, int rank) {
    return std::ranges::find(desc.accepted_ranks, rank) != desc.accepted_ranks.end();
}

[[nodiscard]] std::vector<int64_t>
globalDescriptorShape(std::vector<int64_t> const & input_shape) {
    if (input_shape.empty()) {
        return {};
    }
    return {input_shape.front()};
}

template<typename Params>
[[nodiscard]] Params paramsOrDefault(OutputPipelineStepSpec const & step) {
    Params result;
    if (!step.parameters.has_value()) {
        return result;
    }

    step.parameters->visit([&result](auto const & params) {
        using T = std::decay_t<decltype(params)>;
        if constexpr (std::is_same_v<T, Params>) {
            result = params;
        }
    });
    return result;
}

[[nodiscard]] bool parametersMatchStep(OutputPipelineStepSpec const & step) {
    if (!step.parameters.has_value()) {
        return true;
    }

    bool matches = false;
    step.parameters->visit([&](auto const & params) {
        using T = std::decay_t<decltype(params)>;
        if constexpr (std::is_same_v<T, GlobalAvgPoolModuleParams>) {
            matches = step.step_id == "global_avg_pool";
        } else if constexpr (std::is_same_v<T, SpatialPointModuleParams>) {
            matches = step.step_id == "spatial_point";
        } else if constexpr (std::is_same_v<T, MaskDecoderParams>) {
            matches = step.step_id == "TensorToMask2D";
        } else if constexpr (std::is_same_v<T, PointDecoderParams>) {
            matches = step.step_id == "TensorToPoint2D";
        } else if constexpr (std::is_same_v<T, LineDecoderParams>) {
            matches = step.step_id == "TensorToLine2D";
        } else if constexpr (std::is_same_v<T, FeatureVectorDecoderParams>) {
            matches = step.step_id == "TensorToFeatureVector";
        }
    });
    return matches;
}

}// namespace

std::vector<PipelineStepDescriptor> availablePipelineSteps() {
    return {
            {.step_id = "global_avg_pool",
             .display_name = "Global Average Pool",
             .kind = PipelineStepKind::TensorTransform,
             .accepted_ranks = {3},
             .output_shape = {},
             .output_data_type = {}},
            {.step_id = "spatial_point",
             .display_name = "Spatial Point Extract",
             .kind = PipelineStepKind::TensorTransform,
             .accepted_ranks = {3},
             .output_shape = {},
             .output_data_type = {}},
            {.step_id = "TensorToMask2D",
             .display_name = "Tensor to Mask",
             .kind = PipelineStepKind::TerminalDecoder,
             .accepted_ranks = {3},
             .output_shape = {},
             .output_data_type = "MaskData"},
            {.step_id = "TensorToPoint2D",
             .display_name = "Tensor to Point",
             .kind = PipelineStepKind::TerminalDecoder,
             .accepted_ranks = {3},
             .output_shape = {},
             .output_data_type = "PointData"},
            {.step_id = "TensorToLine2D",
             .display_name = "Tensor to Line",
             .kind = PipelineStepKind::TerminalDecoder,
             .accepted_ranks = {3},
             .output_shape = {},
             .output_data_type = "LineData"},
            {.step_id = "TensorToFeatureVector",
             .display_name = "Tensor to Feature Vector",
             .kind = PipelineStepKind::TerminalDecoder,
             .accepted_ranks = {1},
             .output_shape = {},
             .output_data_type = "TensorData"}};
}

std::optional<PipelineStepDescriptor>
pipelineStepDescriptor(std::string const & step_id) {
    auto const steps = availablePipelineSteps();
    auto const it = std::ranges::find_if(
            steps, [&step_id](PipelineStepDescriptor const & desc) {
                return desc.step_id == step_id;
            });
    if (it == steps.end()) {
        return std::nullopt;
    }
    return *it;
}

bool isTerminalPipelineStep(std::string const & step_id) {
    auto const desc = pipelineStepDescriptor(step_id);
    return desc.has_value() && desc->kind == PipelineStepKind::TerminalDecoder;
}

bool isTensorTransformPipelineStep(std::string const & step_id) {
    auto const desc = pipelineStepDescriptor(step_id);
    return desc.has_value() && desc->kind == PipelineStepKind::TensorTransform;
}

std::string outputDataTypeForStep(std::string const & step_id) {
    auto const desc = pipelineStepDescriptor(step_id);
    return desc.has_value() ? desc->output_data_type : std::string{};
}

SpatialPointModuleParams
spatialPointParamsForStep(OutputPipelineStepSpec const & step) {
    return paramsOrDefault<SpatialPointModuleParams>(step);
}

MaskDecoderParams maskDecoderParamsForStep(OutputPipelineStepSpec const & step) {
    return paramsOrDefault<MaskDecoderParams>(step);
}

PointDecoderParams pointDecoderParamsForStep(OutputPipelineStepSpec const & step) {
    return paramsOrDefault<PointDecoderParams>(step);
}

LineDecoderParams lineDecoderParamsForStep(OutputPipelineStepSpec const & step) {
    return paramsOrDefault<LineDecoderParams>(step);
}

std::optional<std::vector<int64_t>>
propagatePipelineStepShape(std::vector<int64_t> const & input_shape,
                           std::string const & step_id) {
    auto const desc = pipelineStepDescriptor(step_id);
    if (!desc.has_value()) {
        return std::nullopt;
    }

    int const rank = static_cast<int>(input_shape.size());
    if (!acceptsRank(*desc, rank)) {
        return std::nullopt;
    }

    if (desc->kind == PipelineStepKind::TerminalDecoder) {
        return input_shape;
    }

    if (step_id == "global_avg_pool" || step_id == "spatial_point") {
        return globalDescriptorShape(input_shape);
    }

    return input_shape;
}

std::vector<std::string>
validNextPipelineStepIds(std::vector<int64_t> const & input_shape) {
    std::vector<std::string> result;
    int const rank = static_cast<int>(input_shape.size());
    for (auto const & desc: availablePipelineSteps()) {
        if (acceptsRank(desc, rank)) {
            result.push_back(desc.step_id);
        }
    }
    return result;
}

PipelineValidationResult
validateOutputPipeline(std::vector<int64_t> const & input_shape,
                       std::vector<OutputPipelineStepSpec> const & steps) {
    PipelineValidationResult result;
    result.final_tensor_shape = input_shape;

    if (steps.empty()) {
        result.valid = false;
        result.message = "Output pipeline must end with a decoder.";
        return result;
    }

    for (std::size_t i = 0; i < steps.size(); ++i) {
        auto const & step = steps[i];
        auto const desc = pipelineStepDescriptor(step.step_id);
        if (!desc.has_value()) {
            result.valid = false;
            result.message = "Unknown output pipeline step: " + step.step_id;
            return result;
        }

        if (!parametersMatchStep(step)) {
            result.valid = false;
            result.message = "Parameters do not match output pipeline step: " +
                             step.step_id;
            return result;
        }

        if (result.has_terminal) {
            result.valid = false;
            result.message = "No steps may follow terminal decoder: " +
                             steps[i - 1].step_id;
            return result;
        }

        auto next_shape =
                propagatePipelineStepShape(result.final_tensor_shape, step.step_id);
        if (!next_shape.has_value()) {
            result.valid = false;
            result.message = "Step '" + step.step_id +
                             "' is not compatible with tensor rank " +
                             std::to_string(result.final_tensor_shape.size()) + ".";
            return result;
        }

        result.final_tensor_shape = std::move(next_shape.value());
        if (desc->kind == PipelineStepKind::TerminalDecoder) {
            result.has_terminal = true;
            result.output_data_type = desc->output_data_type;
        }
    }

    if (!result.has_terminal) {
        result.valid = false;
        result.message = "Output pipeline must end with a decoder.";
    }
    return result;
}

std::vector<OutputPipelineStepSpec>
defaultOutputPipeline(std::string const & model_id,
                      std::string const & slot_name,
                      std::vector<int64_t> const & output_shape) {
    if (model_id == "general_encoder" || slot_name == "features") {
        if (output_shape.size() == 3) {
            return {OutputPipelineStepSpec{.step_id = "global_avg_pool"},
                    OutputPipelineStepSpec{.step_id = "TensorToFeatureVector"}};
        }
        if (output_shape.size() == 1) {
            return {OutputPipelineStepSpec{.step_id = "TensorToFeatureVector"}};
        }
    }

    auto const valid_steps = validNextPipelineStepIds(output_shape);
    auto const terminal_it = std::ranges::find_if(valid_steps, [](std::string const & id) {
        return isTerminalPipelineStep(id);
    });
    if (terminal_it != valid_steps.end()) {
        return {OutputPipelineStepSpec{.step_id = *terminal_it}};
    }
    return {};
}

}// namespace dl
