/**
 * @file RuntimeModelSpec.hpp
 * @brief JSON-serializable runtime model specification types and helpers.
 */

#ifndef WHISKERTOOLBOX_RUNTIME_MODEL_SPEC_HPP
#define WHISKERTOOLBOX_RUNTIME_MODEL_SPEC_HPP

#include "models_v2/TensorSlotDescriptor.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace dl {

/**
 * @brief JSON-serializable description of one tensor slot (input or output).
 *
 * Maps 1:1 with the JSON schema. Optional fields fall back to
 * `TensorSlotDescriptor` defaults when converting.
 */
struct SlotSpec {
    std::string name;
    std::vector<int64_t> shape;
    std::optional<std::string> description;
    std::optional<std::string> recommended_encoder;
    std::optional<std::vector<OutputPipelineStepSpec>> recommended_pipeline;
    std::optional<bool> is_static;
    std::optional<bool> is_boolean_mask;
    std::optional<int> sequence_dim;

    /**
     * @brief Convert to a TensorSlotDescriptor, applying defaults for omitted fields.
     */
    [[nodiscard]] TensorSlotDescriptor toDescriptor() const;
};

/**
 * @brief JSON-serializable batch mode specification.
 *
 * Maps to `dl::BatchMode` at runtime. Exactly one of `fixed`, `dynamic`,
 * or `recurrent_only` should be set.
 *
 * JSON examples:
 * @code{.json}
 *   "batch_mode": { "fixed": 4 }
 *   "batch_mode": { "dynamic": { "min": 1, "max": 8 } }
 *   "batch_mode": { "recurrent_only": true }
 * @endcode
 */
struct BatchModeSpec {
    /** Fixed(N) */
    std::optional<int> fixed;
    struct DynamicSpec {
        int min = 1;
        /** 0 = unlimited */
        int max = 0;
    };
    /** Dynamic(min, max) */
    std::optional<DynamicSpec> dynamic;
    /** RecurrentOnly */
    std::optional<bool> recurrent_only;

    /**
     * @brief Convert to a BatchMode variant, defaulting to Dynamic(1, 0).
     */
    [[nodiscard]] BatchMode toBatchMode() const;
};

/**
 * @brief JSON-serializable weights variant for multi-batch-size model loading.
 *
 * Each variant specifies a weights file and the batch size it was compiled
 * for. The widget auto-selects the variant matching the active batch size.
 *
 * JSON example:
 * @code{.json}
 *   "weights_variants": [
 *     { "path": "model_batch1.pt2", "batch_size": 1, "label": "recurrent" },
 *     { "path": "model_batch8.pt2", "batch_size": 8, "label": "batched" }
 *   ]
 * @endcode
 */
struct WeightsVariant {
    /** Path to weights file */
    std::string path;
    /** Batch size this variant was compiled for */
    int batch_size = 1;
    /** Optional human-readable label */
    std::optional<std::string> label;
    /** Optional backend override */
    std::optional<std::string> backend;
};

/**
 * @brief JSON-serializable specification for a single post-encoder module step.
 *
 * Used within the `"post_encoder"` array of a `RuntimeModelSpec`. Each entry
 * specifies a module by `"module"` key and optional parameters.
 *
 * JSON examples:
 * @code{.json}
 *   "post_encoder": [
 *     { "module": "global_avg_pool" }
 *   ]
 *
 *   "post_encoder": [
 *     { "module": "spatial_point", "interpolation": "bilinear" }
 *   ]
 * @endcode
 */
struct PostEncoderStepSpec {
    /** Module key (e.g. "global_avg_pool") */
    std::string module;
    /** "nearest" | "bilinear" (spatial_point only) */
    std::optional<std::string> interpolation;
};

/**
 * @brief JSON-serializable specification for a runtime-defined model.
 *
 * Allows users to specify an ExecuTorch model's inputs and outputs via JSON
 * at runtime, without recompilation.
 *
 * JSON example:
 * @code{.json}
 * {
 *   "model_id": "my_tracker",
 *   "display_name": "My Tracker",
 *   "description": "Tracks whisker tips",
 *   "weights_path": "model.pte",
 *   "preferred_batch_size": 1,
 *   "max_batch_size": 1,
 *   "batch_mode": { "dynamic": { "min": 1, "max": 8 } },
 *   "weights_variants": [
 *     { "path": "model_batch1.pt2", "batch_size": 1, "label": "recurrent" },
 *     { "path": "model_batch8.pt2", "batch_size": 8, "label": "batched" }
 *   ],
 *   "inputs": [
 *     { "name": "image", "shape": [3, 256, 256], "recommended_encoder": "ImageEncoder" }
 *   ],
 *   "outputs": [
 *     {
 *       "name": "heatmap",
 *       "shape": [1, 256, 256],
 *       "recommended_pipeline": [{ "step_id": "TensorToMask2D" }]
 *     }
 *   ],
 *   "post_encoder": [
 *     { "module": "global_avg_pool" }
 *   ]
 * }
 * @endcode
 */
struct RuntimeModelSpec {
    std::string model_id;
    std::string display_name;
    std::optional<std::string> description;
    std::optional<std::string> weights_path;
    /** "auto", "torchscript", "aotinductor", "executorch" */
    std::optional<std::string> backend;
    std::optional<int> preferred_batch_size;
    std::optional<int> max_batch_size;
    /** Rich batch-size constraint */
    std::optional<BatchModeSpec> batch_mode;
    /** Multi-variant weights */
    std::optional<std::vector<WeightsVariant>> weights_variants;
    std::vector<SlotSpec> inputs;
    std::vector<SlotSpec> outputs;
    /** Optional post-encoder pipeline */
    std::optional<std::vector<PostEncoderStepSpec>> post_encoder;

    /**
     * @brief Parse a RuntimeModelSpec from a JSON string.
     */
    [[nodiscard]] static rfl::Result<RuntimeModelSpec>
    fromJson(std::string const & json_str);

    /**
     * @brief Parse a RuntimeModelSpec from a JSON file.
     *
     * Relative `weights_path` values are resolved against the file's directory.
     */
    [[nodiscard]] static rfl::Result<RuntimeModelSpec>
    fromJsonFile(std::filesystem::path const & path);

    /**
     * @brief Serialize this spec to a JSON string.
     */
    [[nodiscard]] std::string toJson() const;

    /**
     * @brief Convert all input SlotSpecs to TensorSlotDescriptors.
     */
    [[nodiscard]] std::vector<TensorSlotDescriptor> inputDescriptors() const;

    /**
     * @brief Convert all output SlotSpecs to TensorSlotDescriptors.
     */
    [[nodiscard]] std::vector<TensorSlotDescriptor> outputDescriptors() const;

    /**
     * @brief Validate semantic correctness beyond JSON schema.
     *
     * @return Error messages; empty if valid.
     */
    [[nodiscard]] std::vector<std::string> validate() const;
};

}// namespace dl

#endif// WHISKERTOOLBOX_RUNTIME_MODEL_SPEC_HPP
