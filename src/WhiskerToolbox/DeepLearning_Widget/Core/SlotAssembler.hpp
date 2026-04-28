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

#include "BatchInferenceResult.hpp"
#include "models_v2/TensorSlotDescriptor.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct SlotBindingData;
struct StaticInputData;
struct OutputBindingData;
struct RecurrentBindingData;
struct ImageSize;

class DataManager;
class MediaData;

/**
 * @brief Lightweight model metadata for display in the UI.
 * 
 * Mirrors dl::ModelRegistry::ModelInfo but without any libtorch dependency.
 */
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

/**
 * @brief Bridge between DataManager data and model tensor I/O.
 * 
 * Owns a cached ModelBase instance behind a PIMPL to prevent libtorch
 * headers from leaking into Qt translation units.
 */
class SlotAssembler {
public:
    SlotAssembler();
    ~SlotAssembler();
    SlotAssembler(SlotAssembler &&) noexcept;
    SlotAssembler & operator=(SlotAssembler &&) noexcept;

    // ── Instance: model lifecycle ──────────────────────────────────────────

    /**
     * @brief Load a model by ID from the registry.  Resets previous weights.
     * @param model_id  Model ID to load.
     * @return true if the model ID was found and instantiated.
     */
    bool loadModel(std::string const & model_id);

    /**
     * @brief Load weights from a file path into the current model.
     * @param weights_path  Path to the weights file.
     * @return true on success.
     */
    bool loadWeights(std::string const & weights_path);

    /**
     * @brief Run a dummy forward pass to validate weight compatibility.
     * 
     * Creates zero-filled input tensors matching each input slot's shape
     * (batch_size=1), calls forward(), and checks that outputs are produced.
     * Uses torch::NoGradGuard to avoid unnecessary gradient computation.
     * 
     * @pre isModelReady() must be true.
     * @return Empty string on success, or a diagnostic message on mismatch.
     */
    [[nodiscard]] std::string validateWeights();

    /**
     * @brief Whether a model is loaded AND its weights are active.
     */
    [[nodiscard]] bool isModelReady() const;

    /**
     * @brief Currently loaded model ID, or empty.
     */
    [[nodiscard]] std::string const & currentModelId() const;

    /**
     * @brief Clear the current model and free resources.
     */
    void resetModel();

    // ── Instance: inference ────────────────────────────────────────────────

    /**
     * @brief Run a single-frame inference pipeline:
     * assemble inputs → forward → decode outputs.
     * @param dm DataManager for encoding inputs / writing outputs
     * @param input_bindings Dynamic input slot bindings
     * @param static_inputs Static (memory) input entries
     * @param output_bindings Output slot bindings
     * @param current_frame Current frame index
     * @param source_image_size Original image dimensions
     * @throws std::runtime_error on failure.
     */
    void runSingleFrame(
            DataManager & dm,
            std::vector<SlotBindingData> const & input_bindings,
            std::vector<StaticInputData> const & static_inputs,
            std::vector<OutputBindingData> const & output_bindings,
            int current_frame,
            ImageSize source_image_size);

    /**
     * @brief Callback type for progress reporting during recurrent inference.
     * @param current_frame 0-based index of the frame being processed
     * @param total_frames Total number of frames to process
     */
    using ProgressCallback = std::function<void(int current_frame, int total_frames)>;

    /**
     * @brief Callback invoked per-frame with decoded results for progressive delivery.
     * When provided to runBatchRangeOffline(), results are pushed via this
     * callback instead of accumulating in BatchInferenceResult::results.
     */
    using ResultCallback = std::function<void(std::vector<FrameResult>)>;

    /**
     * @brief Run independent (non-recurrent) inference over a range of frames.
     * @param dm DataManager for encoding inputs / writing outputs
     * @param input_bindings Dynamic input slot bindings
     * @param static_inputs Static (memory) input entries
     * @param output_bindings Output slot bindings
     * @param start_frame First frame to process (inclusive)
     * @param end_frame Last frame to process (inclusive)
     * @param source_image_size Original image dimensions
     * @param batch_size Number of frames to process per forward pass.
     * @param progress Optional callback for progress reporting
     * @throws std::runtime_error on inference failure
     */
    void runBatchRange(
            DataManager & dm,
            std::vector<SlotBindingData> const & input_bindings,
            std::vector<StaticInputData> const & static_inputs,
            std::vector<OutputBindingData> const & output_bindings,
            int start_frame,
            int end_frame,
            ImageSize source_image_size,
            int batch_size = 1,
            ProgressCallback const & progress = nullptr);

    /**
     * @brief Map from DataManager data_key to a cloned MediaData instance.
     * 
     * Used by runBatchRangeOffline() so the worker thread has its own
     * independent MediaData (e.g. a separate VideoData with its own
     * FFmpeg decoder state) while the UI continues using the original.
     */
    using MediaOverrides =
            std::unordered_map<std::string, std::shared_ptr<MediaData>>;
    /** 
     * @brief Run independent inference over a frame range without writing to
     * DataManager.
     * 
     * Identical to runBatchRange() except:
     *  - Uses @p media_overrides for MediaData lookups (thread-safe clone).
     *  - Returns decoded results in a BatchInferenceResult instead of
     *    calling addAtTime() on DataManager.
     *  - Checks @p cancel_requested before each frame for early exit.
     * 
     * @param dm DataManager for non-media input encoding (masks, points, lines)
     * @param media_overrides Cloned MediaData instances keyed by data_key
     * @param input_bindings Dynamic input slot bindings
     * @param static_inputs Static (memory) input entries
     * @param output_bindings Output slot bindings
     * @param start_frame First frame to process (inclusive)
     * @param end_frame Last frame to process (inclusive)
     * @param source_image_size Original image dimensions
     * @param cancel_requested Checked before each frame; stops early if true
     * @param batch_size Number of frames to process per forward pass.
     *        Must be >= 1. The last chunk may contain fewer frames.
     * @param progress Optional callback for progress reporting
     * @param result_callback Optional per-frame result callback for progressive
     *        delivery.  When non-null, decoded outputs are pushed via this
     *        callback and NOT accumulated in BatchInferenceResult::results.
     * @return Accumulated decoded results (may be partial on cancellation).
     *         If result_callback is set, the results vector will be empty.
     */
    [[nodiscard]] BatchInferenceResult runBatchRangeOffline(
            DataManager & dm,
            MediaOverrides const & media_overrides,
            std::vector<SlotBindingData> const & input_bindings,
            std::vector<StaticInputData> const & static_inputs,
            std::vector<OutputBindingData> const & output_bindings,
            int start_frame,
            int end_frame,
            ImageSize source_image_size,
            std::atomic<bool> const & cancel_requested,
            int batch_size = 1,
            ProgressCallback const & progress = nullptr,
            ResultCallback const & result_callback = nullptr);

    /**
     * @brief Run sequential recurrent inference over a range of frames.
     * 
     * For each frame in [start_frame, start_frame + frame_count):
     *   1. Assemble dynamic + static inputs
     *   2. Inject recurrent tensors (output from previous frame) into
     *      the corresponding input slots
     *   3. Call forward()
     *   4. Decode outputs into DataManager
     *   5. Cache output tensors for the next frame's recurrent inputs
     *
     * Batch size is forced to 1 during recurrent inference.
     *
     * @param dm DataManager for encoding inputs / writing outputs
     * @param input_bindings Dynamic input slot bindings
     * @param static_inputs Static (memory) input entries
     * @param output_bindings Output slot bindings
     * @param recurrent_bindings Recurrent feedback bindings
     * @param start_frame First frame to process
     * @param frame_count Number of frames to process
     * @param source_image_size Original image dimensions
     * @param progress Optional callback for progress reporting
     * @throws std::runtime_error on inference failure
     */
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

    // ── Instance: device context ─────────────────────────────────────────

    /**
     * @brief Whether CUDA is available on this system (compile-time + runtime).
     */
    [[nodiscard]] static bool isCudaAvailable();

    /**
     * @brief Human-readable name of the active device ("GPU (CUDA)" or "CPU").
     */
    [[nodiscard]] static std::string currentDeviceName();

    /**
     * @brief Set the active device by name: "cuda" or "cpu".
     */
    static void setDeviceByName(std::string const & name);

    /**
     * @brief Initialize the deep-learning device context on the calling thread.
     * 
     * On CUDA builds this ensures the CUDA runtime context is set for the
     * current thread — required on Windows where CUDA per-thread state is
     * not inherited from the main thread. Safe to call from any thread;
     * no-op when the active device is CPU.
     * 
     * Call at the start of worker thread `run()` methods before any
     * libtorch operations.
     */
    static void initDeviceForCurrentThread();

    // ── Instance: static tensor cache ──────────────────────────────────────

    /**
     * @brief Capture a static input at a specific frame and store in the cache.
     * 
     * Encodes the data from DataManager at the given frame using the model's
     * recommended encoder for the slot, then stores the resulting tensor.
     * The cached tensor is reused during inference for Absolute-mode entries.
     * 
     * @param dm DataManager to fetch data from
     * @param entry Static input entry describing what to capture
     * @param frame The frame number to encode and cache
     * @param source_image_size Original image dimensions for coordinate scaling
     * @return true if the tensor was successfully captured and cached
     */
    bool captureStaticInput(
            DataManager & dm,
            StaticInputData const & entry,
            int frame,
            ImageSize source_image_size);

    /**
     * @brief Clear a single entry from the static tensor cache.
     */
    void clearStaticCacheEntry(std::string const & cache_key);

    /**
     * @brief Clear the entire static tensor cache.
     */
    void clearStaticCache();

    // ── Instance: recurrent tensor cache ──────────────────────────────────────────

    /**
     * @brief Clear the recurrent tensor cache.
     * 
     * Call when changing models, starting a new sequence, or clearing state.
     */
    void clearRecurrentCache();

    /**
     * @brief Whether a cached tensor exists for the given key.
     */
    [[nodiscard]] bool hasStaticCacheEntry(std::string const & cache_key) const;

    /**
     * @brief List all keys currently in the static tensor cache.
     */
    [[nodiscard]] std::vector<std::string> staticCacheKeys() const;

    /**
     * @brief Get the dimensions of a cached tensor as a vector of int64_t.
     * 
     * @returns The dimensions of the cached tensor as a vector of int64_t.
     *          Returns empty vector if the key is not in the cache.
     */
    [[nodiscard]] std::vector<int64_t> staticCacheTensorShape(
            std::string const & cache_key) const;

    /**
     * @brief Get min/max values of a cached tensor for preview display.
     * 
     * @returns The min and max values of the cached tensor as a pair of float.
     *          Returns {0.0f, 0.0f} if the key is not in the cache.
     */
    [[nodiscard]] std::pair<float, float> staticCacheTensorRange(
            std::string const & cache_key) const;

    // ── Static: registry queries ───────────────────────────────────────────

    /**
     * @brief List all registered model IDs.
     */
    [[nodiscard]] static std::vector<std::string> availableModelIds();

    /**
     * @brief Get display metadata for a registered model.
     */
    [[nodiscard]] static std::optional<ModelDisplayInfo> getModelDisplayInfo(
            std::string const & model_id);

    /**
     * @brief Get display metadata from the currently loaded model instance.
     * 
     * Unlike the static `getModelDisplayInfo()`, this reflects any runtime
     * reconfiguration (e.g. changed input resolution or output shape).
     */
    [[nodiscard]] std::optional<ModelDisplayInfo> currentModelDisplayInfo() const;

    // ── Static: encoder / decoder queries ──────────────────────────────────

    /**
     * @brief Available encoder names (e.g. "ImageEncoder", "Point2DEncoder", …).
     */
    [[nodiscard]] static std::vector<std::string> availableEncoders();

    /**
     * @brief Available decoder names (e.g. "TensorToMask2D", …).
     */
    [[nodiscard]] static std::vector<std::string> availableDecoders();

    /**
     * @brief Map encoder name → DataManager data type name for combo filtering.
     * 
     * @returns "MediaData", "PointData", "MaskData", "LineData", or "".
     */
    [[nodiscard]] static std::string dataTypeForEncoder(
            std::string const & encoder_id);

    /**
     * @brief Map decoder name → DataManager data type name for combo filtering.
     */
    [[nodiscard]] static std::string dataTypeForDecoder(
            std::string const & decoder_id);

    // ── Post-encoder configuration ─────────────────────────────────────────

    /**
     * @brief Configure a post-encoder module on the currently loaded model.
     * 
     * Only applies when the current model is a `GeneralEncoderModel`.
     * Calling this when the model changes will reset the configuration.
     * 
     * @param module_type  Module key: "" / "none", "global_avg_pool",
     *                     or "spatial_point".
     * @param source_image_size  Source image size used for "spatial_point"
     *        coordinate scaling.
     * @param interpolation  "nearest" or "bilinear" (spatial_point only).
     */
    void configurePostEncoderModule(
            std::string const & module_type,
            ImageSize source_image_size = {},
            std::string const & interpolation = "nearest");

    /**
     * @brief Update the spatial-point query for the current frame.
     * 
     * For the "spatial_point" post-encoder, reads the point at
     * `frame` from `point_key` in DataManager and forwards it to the module.
     * 
     * No-op if the current module is not "spatial_point" or if `point_key`
     * is empty.
     */
    void updateSpatialPoint(DataManager & dm,
                            std::string const & point_key,
                            int frame);

    // ── Model shape configuration ──────────────────────────────────────────
    /**
     * @brief Reconfigure input resolution and output shape on the current model.
     * 
     * Only applies when the current model is a `GeneralEncoderModel`.
     * Must be called before `loadWeights()` or inference.
     * 
     * @param input_height  Input image height in pixels (> 0).
     * @param input_width   Input image width in pixels (> 0).
     * @param output_shape  Raw encoder output shape (excluding batch dim).
     *                      If empty, only input resolution is changed.
     */
    void configureModelShape(
            int input_height,
            int input_width,
            std::vector<int64_t> const & output_shape = {});

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

#endif// DEEP_LEARNING_SLOT_ASSEMBLER_HPP
