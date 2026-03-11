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

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct SlotBindingData;
struct StaticInputData;
struct OutputBindingData;
struct RecurrentBindingData;
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
    dl::BatchMode batch_mode = dl::DynamicBatch{1, 0};///< Rich batch-size constraint
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

    /// Callback type for progress reporting during recurrent inference.
    /// @param current_frame 0-based index of the frame being processed
    /// @param total_frames Total number of frames to process
    using ProgressCallback = std::function<void(int current_frame, int total_frames)>;

    /// Run sequential recurrent inference over a range of frames.
    ///
    /// For each frame in [start_frame, start_frame + frame_count):
    ///   1. Assemble dynamic + static inputs
    ///   2. Inject recurrent tensors (output from previous frame) into
    ///      the corresponding input slots
    ///   3. Call forward()
    ///   4. Decode outputs into DataManager
    ///   5. Cache output tensors for the next frame's recurrent inputs
    ///
    /// Batch size is forced to 1 during recurrent inference.
    ///
    /// @param dm DataManager for encoding inputs / writing outputs
    /// @param input_bindings Dynamic input slot bindings
    /// @param static_inputs Static (memory) input entries
    /// @param output_bindings Output slot bindings
    /// @param recurrent_bindings Recurrent feedback bindings
    /// @param start_frame First frame to process
    /// @param frame_count Number of frames to process
    /// @param source_image_size Original image dimensions
    /// @param progress Optional callback for progress reporting
    /// @throws std::runtime_error on inference failure
    void runRecurrentSequence(
            DataManager & dm,
            std::vector<SlotBindingData> const & input_bindings,
            std::vector<StaticInputData> const & static_inputs,
            std::vector<OutputBindingData> const & output_bindings,
            std::vector<RecurrentBindingData> const & recurrent_bindings,
            int start_frame,
            int frame_count,
            ImageSize source_image_size,
            ProgressCallback const & progress = nullptr);

    // ── Instance: static tensor cache ──────────────────────────────────────

    /// Capture a static input at a specific frame and store in the cache.
    ///
    /// Encodes the data from DataManager at the given frame using the model's
    /// recommended encoder for the slot, then stores the resulting tensor.
    /// The cached tensor is reused during inference for Absolute-mode entries.
    ///
    /// @param dm DataManager to fetch data from
    /// @param entry Static input entry describing what to capture
    /// @param frame The frame number to encode and cache
    /// @param source_image_size Original image dimensions for coordinate scaling
    /// @return true if the tensor was successfully captured and cached
    bool captureStaticInput(
            DataManager & dm,
            StaticInputData const & entry,
            int frame,
            ImageSize source_image_size);

    /// Clear a single entry from the static tensor cache.
    void clearStaticCacheEntry(std::string const & cache_key);

    /// Clear the entire static tensor cache.
    void clearStaticCache();

    // ── Instance: recurrent tensor cache ──────────────────────────────────────────

    /// Clear the recurrent tensor cache.
    /// Call when changing models, starting a new sequence, or clearing state.
    void clearRecurrentCache();

    /// Whether a cached tensor exists for the given key.
    [[nodiscard]] bool hasStaticCacheEntry(std::string const & cache_key) const;

    /// List all keys currently in the static tensor cache.
    [[nodiscard]] std::vector<std::string> staticCacheKeys() const;

    /// Get the dimensions of a cached tensor as a vector of int64_t.
    /// Returns empty vector if the key is not in the cache.
    [[nodiscard]] std::vector<int64_t> staticCacheTensorShape(
            std::string const & cache_key) const;

    /// Get min/max values of a cached tensor for preview display.
    /// Returns {0, 0} if the key is not in the cache.
    [[nodiscard]] std::pair<float, float> staticCacheTensorRange(
            std::string const & cache_key) const;

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

#endif// DEEP_LEARNING_SLOT_ASSEMBLER_HPP
