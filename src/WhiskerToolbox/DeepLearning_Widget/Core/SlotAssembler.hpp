#ifndef DEEP_LEARNING_SLOT_ASSEMBLER_HPP
#define DEEP_LEARNING_SLOT_ASSEMBLER_HPP

/**
 * @file SlotAssembler.hpp
 * @brief Bridge between UI/DataManager and the DeepLearning library.
 *
 * This class isolates all libtorch usage behind a PIMPL firewall so that
 * Qt widget code never includes <torch/torch.h> — preventing the infamous
 * Qt `slots` macro conflict with libtorch.
 *
 * Instance methods manage a cached ModelBase for weights loading and
 * inference. Static methods provide read-only queries over the model
 * registry and encoder/decoder factories.
 *
 * @see ChannelEncoder, ChannelDecoder for the encoding/decoding interface.
 * @see ModelBase for the model forward pass interface.
 */

#include "models_v2/TensorSlotDescriptor.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

// Forward declarations to minimise coupling
struct SlotBindingData;
struct StaticInputData;
struct OutputBindingData;
struct ImageSize;

class DataManager;

/// Lightweight model metadata for display in the UI.
/// Mirrors dl::ModelRegistry::ModelInfo but without any libtorch dependency.
struct ModelDisplayInfo {
    std::string model_id;
    std::string display_name;
    std::string description;
    std::vector<dl::TensorSlotDescriptor> inputs;
    std::vector<dl::TensorSlotDescriptor> outputs;
    int preferred_batch_size = 0;
    int max_batch_size = 0;
};

/// Bridge between DataManager data and model tensor I/O.
///
/// Owns a cached ModelBase instance behind a PIMPL to prevent libtorch
/// headers from leaking into Qt translation units.
class SlotAssembler {
public:
    SlotAssembler();
    ~SlotAssembler();
    SlotAssembler(SlotAssembler &&) noexcept;
    SlotAssembler & operator=(SlotAssembler &&) noexcept;

    // ── Instance: model lifecycle ──────────────────────────────────────────

    /// Load a model by ID from the registry.  Resets previous weights.
    /// @return true if the model ID was found and instantiated.
    bool loadModel(std::string const & model_id);

    /// Load weights from a file path into the current model.
    /// @return true on success.
    bool loadWeights(std::string const & weights_path);

    /// Whether a model is loaded AND its weights are active.
    [[nodiscard]] bool isModelReady() const;

    /// Currently loaded model ID, or empty.
    [[nodiscard]] std::string const & currentModelId() const;

    /// Clear the current model and free resources.
    void resetModel();

    // ── Instance: inference ────────────────────────────────────────────────

    /// Run a single-frame inference pipeline:
    /// assemble inputs → forward → decode outputs.
    /// @throws std::runtime_error on failure.
    void runSingleFrame(
        DataManager & dm,
        std::vector<SlotBindingData> const & input_bindings,
        std::vector<StaticInputData> const & static_inputs,
        std::vector<OutputBindingData> const & output_bindings,
        int current_frame,
        ImageSize source_image_size);

    // ── Static: registry queries ───────────────────────────────────────────

    /// List all registered model IDs.
    [[nodiscard]] static std::vector<std::string> availableModelIds();

    /// Get display metadata for a registered model.
    [[nodiscard]] static std::optional<ModelDisplayInfo> getModelDisplayInfo(
        std::string const & model_id);

    // ── Static: encoder / decoder queries ──────────────────────────────────

    /// Available encoder names (e.g. "ImageEncoder", "Point2DEncoder", …).
    [[nodiscard]] static std::vector<std::string> availableEncoders();

    /// Available decoder names (e.g. "TensorToMask2D", …).
    [[nodiscard]] static std::vector<std::string> availableDecoders();

    /// Map encoder name → DataManager data type name for combo filtering.
    /// Returns "MediaData", "PointData", "MaskData", "LineData", or "".
    [[nodiscard]] static std::string dataTypeForEncoder(
        std::string const & encoder_id);

    /// Map decoder name → DataManager data type name for combo filtering.
    [[nodiscard]] static std::string dataTypeForDecoder(
        std::string const & decoder_id);

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

#endif // DEEP_LEARNING_SLOT_ASSEMBLER_HPP
