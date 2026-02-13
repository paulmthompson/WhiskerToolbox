#ifndef DEEP_LEARNING_BINDING_DATA_HPP
#define DEEP_LEARNING_BINDING_DATA_HPP

/**
 * @file DeepLearningBindingData.hpp
 * @brief Pure-data structs for model slot bindings.
 *
 * These are deliberately kept in a separate header from DeepLearningState
 * so that SlotAssembler.cpp (which includes libtorch) can use them
 * without pulling in Qt headers via EditorState.
 */

#include <string>
#include <vector>

/// Serializable binding for a dynamic (per-frame) model input slot.
struct SlotBindingData {
    std::string slot_name;          ///< Model input slot name (e.g. "encoder_image")
    std::string data_key;           ///< DataManager key (e.g. "media/video_1")
    std::string encoder_id;         ///< Encoder factory key (e.g. "ImageEncoder")
    std::string mode = "Raw";       ///< "Raw", "Binary", "Heatmap"
    float gaussian_sigma = 2.0f;    ///< Gaussian sigma (Heatmap mode only)
};

/// Serializable binding for a model output slot.
struct OutputBindingData {
    std::string slot_name;          ///< Model output slot name
    std::string data_key;           ///< DataManager key to write results into
    std::string decoder_id;         ///< Decoder factory key (e.g. "TensorToMask2D")
    float threshold = 0.5f;         ///< Mask/line threshold
    bool subpixel = true;           ///< Point subpixel refinement
};

/// Serializable entry for a static (memory) model input.
struct StaticInputData {
    std::string slot_name;          ///< Static slot name (e.g. "memory_images")
    int memory_index = 0;           ///< Position in the memory buffer
    std::string data_key;           ///< DataManager source key
    int time_offset = 0;            ///< Relative frame offset (e.g. -1)
    bool active = true;             ///< For boolean mask slots
};

#endif // DEEP_LEARNING_BINDING_DATA_HPP
