/**
 * @file OutputPipeline.hpp
 * @brief Metadata and compatibility helpers for deep-learning output pipelines.
 */

#ifndef WHISKERTOOLBOX_OUTPUT_PIPELINE_HPP
#define WHISKERTOOLBOX_OUTPUT_PIPELINE_HPP

#include "channel_decoding/ChannelDecoder.hpp"
#include "post_encoder/PostEncoderModuleFactory.hpp"

#include <rfl.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace dl {

/**
 * @brief Type-erased, serializable parameters for one output pipeline step.
 *
 * The `step_id` selects the registered factory entry, while this variant holds
 * the parameter struct owned by that concrete transform or decoder.
 */
using OutputPipelineStepParameters = rfl::TaggedUnion<
        "parameter_type",
        GlobalAvgPoolModuleParams,
        SpatialPointModuleParams,
        MaskDecoderParams,
        PointDecoderParams,
        LineDecoderParams,
        FeatureVectorDecoderParams>;

/**
 * @brief Serializable description of one output pipeline step.
 */
struct OutputPipelineStepSpec {
    std::string step_id;///< Factory key for the step.
    std::optional<OutputPipelineStepParameters>
            parameters;///< Step-specific transform or decoder parameters.
};

/**
 * @brief Category of a pipeline step.
 */
enum class PipelineStepKind {
    TensorTransform,
    TerminalDecoder
};

/**
 * @brief Declarative metadata for one available output pipeline step.
 */
struct PipelineStepDescriptor {
    std::string step_id;              ///< Factory key, e.g. "global_avg_pool".
    std::string display_name;         ///< User-facing label.
    PipelineStepKind kind;            ///< Tensor transform or terminal decoder.
    std::vector<int> accepted_ranks;  ///< Accepted tensor rank excluding batch.
    std::vector<int64_t> output_shape;///< Fixed output shape override, if any.
    std::string output_data_type;     ///< DataManager type for terminal decoders.
};

/**
 * @brief Result of validating and propagating an output pipeline.
 */
struct PipelineValidationResult {
    bool valid = true;
    std::string message;
    std::vector<int64_t> final_tensor_shape;
    std::string output_data_type;
    bool has_terminal = false;
};

/**
 * @brief Return metadata for every registered output pipeline step.
 */
[[nodiscard]] std::vector<PipelineStepDescriptor> availablePipelineSteps();

/**
 * @brief Look up one pipeline step descriptor by id.
 */
[[nodiscard]] std::optional<PipelineStepDescriptor>
pipelineStepDescriptor(std::string const & step_id);

/**
 * @brief Return true when @p step_id names a terminal decoder.
 */
[[nodiscard]] bool isTerminalPipelineStep(std::string const & step_id);

/**
 * @brief Return true when @p step_id names a tensor transform.
 */
[[nodiscard]] bool isTensorTransformPipelineStep(std::string const & step_id);

/**
 * @brief Return the DataManager type produced by a terminal pipeline step.
 */
[[nodiscard]] std::string outputDataTypeForStep(std::string const & step_id);

/**
 * @brief Extract spatial-point transform parameters from a step.
 *
 * @post Missing or mismatched parameters return default spatial-point settings.
 */
[[nodiscard]] SpatialPointModuleParams
spatialPointParamsForStep(OutputPipelineStepSpec const & step);

/**
 * @brief Extract mask decoder parameters from a step.
 *
 * @post Missing or mismatched parameters return default mask decoder settings.
 */
[[nodiscard]] MaskDecoderParams
maskDecoderParamsForStep(OutputPipelineStepSpec const & step);

/**
 * @brief Extract point decoder parameters from a step.
 *
 * @post Missing or mismatched parameters return default point decoder settings.
 */
[[nodiscard]] PointDecoderParams
pointDecoderParamsForStep(OutputPipelineStepSpec const & step);

/**
 * @brief Extract line decoder parameters from a step.
 *
 * @post Missing or mismatched parameters return default line decoder settings.
 */
[[nodiscard]] LineDecoderParams
lineDecoderParamsForStep(OutputPipelineStepSpec const & step);

/**
 * @brief Propagate shape metadata through a single tensor transform step.
 *
 * @pre input_shape must describe tensor rank excluding batch.
 */
[[nodiscard]] std::optional<std::vector<int64_t>>
propagatePipelineStepShape(std::vector<int64_t> const & input_shape,
                           std::string const & step_id);

/**
 * @brief Return step ids that can follow the current tensor shape.
 */
[[nodiscard]] std::vector<std::string>
validNextPipelineStepIds(std::vector<int64_t> const & input_shape);

/**
 * @brief Validate a complete output pipeline and propagate its metadata.
 */
[[nodiscard]] PipelineValidationResult
validateOutputPipeline(std::vector<int64_t> const & input_shape,
                       std::vector<OutputPipelineStepSpec> const & steps);

/**
 * @brief Build a conservative default pipeline for a model output.
 */
[[nodiscard]] std::vector<OutputPipelineStepSpec>
defaultOutputPipeline(std::string const & model_id,
                      std::string const & slot_name,
                      std::vector<int64_t> const & output_shape);

}// namespace dl

#endif// WHISKERTOOLBOX_OUTPUT_PIPELINE_HPP
