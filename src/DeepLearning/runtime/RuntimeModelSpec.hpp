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

/// JSON-serializable description of one tensor slot (input or output).
///
/// Maps 1:1 with the JSON schema. Optional fields fall back to
/// `TensorSlotDescriptor` defaults when converting.
struct SlotSpec {
    std::string name;
    std::vector<int64_t> shape;
    std::optional<std::string> description;
    std::optional<std::string> recommended_encoder;
    std::optional<std::string> recommended_decoder;
    std::optional<bool> is_static;
    std::optional<bool> is_boolean_mask;
    std::optional<int> sequence_dim;

    /// Convert to a TensorSlotDescriptor, applying defaults for omitted fields.
    [[nodiscard]] TensorSlotDescriptor toDescriptor() const;
};

/// JSON-serializable specification for a runtime-defined model.
///
/// Allows users to specify an ExecuTorch model's inputs and outputs via JSON
/// at runtime, without recompilation.
///
/// JSON example:
/// @code{.json}
/// {
///   "model_id": "my_tracker",
///   "display_name": "My Tracker",
///   "description": "Tracks whisker tips",
///   "weights_path": "model.pte",
///   "preferred_batch_size": 1,
///   "max_batch_size": 1,
///   "inputs": [
///     { "name": "image", "shape": [3, 256, 256], "recommended_encoder": "ImageEncoder" }
///   ],
///   "outputs": [
///     { "name": "heatmap", "shape": [1, 256, 256], "recommended_decoder": "TensorToMask2D" }
///   ]
/// }
/// @endcode
struct RuntimeModelSpec {
    std::string model_id;
    std::string display_name;
    std::optional<std::string> description;
    std::optional<std::string> weights_path;
    std::optional<int> preferred_batch_size;
    std::optional<int> max_batch_size;
    std::vector<SlotSpec> inputs;
    std::vector<SlotSpec> outputs;

    /// Parse a RuntimeModelSpec from a JSON string.
    [[nodiscard]] static rfl::Result<RuntimeModelSpec>
    fromJson(std::string const & json_str);

    /// Parse a RuntimeModelSpec from a JSON file.
    /// Relative `weights_path` values are resolved against the file's directory.
    [[nodiscard]] static rfl::Result<RuntimeModelSpec>
    fromJsonFile(std::filesystem::path const & path);

    /// Serialize this spec to a JSON string.
    [[nodiscard]] std::string toJson() const;

    /// Convert all input SlotSpecs to TensorSlotDescriptors.
    [[nodiscard]] std::vector<TensorSlotDescriptor> inputDescriptors() const;

    /// Convert all output SlotSpecs to TensorSlotDescriptors.
    [[nodiscard]] std::vector<TensorSlotDescriptor> outputDescriptors() const;

    /// Validate semantic correctness beyond JSON schema.
    /// Returns error messages; empty if valid.
    [[nodiscard]] std::vector<std::string> validate() const;
};

} // namespace dl

#endif // WHISKERTOOLBOX_RUNTIME_MODEL_SPEC_HPP
