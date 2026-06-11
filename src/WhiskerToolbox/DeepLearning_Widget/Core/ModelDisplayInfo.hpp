#ifndef MODEL_DISPLAY_INFO_HPP
#define MODEL_DISPLAY_INFO_HPP


#include "models_v2/TensorSlotDescriptor.hpp"  // TensorSlotDescriptor, DynamicBatch, BatchMode

#include <string>
#include <vector>

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


#endif// MODEL_DISPLAY_INFO_HPP