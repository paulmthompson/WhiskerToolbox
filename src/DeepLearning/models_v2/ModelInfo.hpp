/**
 * @file ModelInfo.hpp
 * @brief Torch-free aggregated metadata for a deep-learning model.
 */

#ifndef NEURALYZER_MODEL_INFO_HPP
#define NEURALYZER_MODEL_INFO_HPP

#include "TensorSlotDescriptor.hpp"

#include <string>
#include <vector>

namespace dl {

/**
 * @brief Aggregated metadata for a registered or loaded model.
 *
 * Populated by `ModelRegistry::getModelInfo()` for registered models, or by
 * `SlotAssembler::currentModelInfo()` for the live loaded instance (which may
 * reflect runtime reconfiguration such as post-encoder output shapes).
 */
struct ModelInfo {
    std::string model_id;
    std::string display_name;
    std::string description;
    std::vector<TensorSlotDescriptor> inputs;
    std::vector<TensorSlotDescriptor> outputs;
    int preferred_batch_size = 0;
    int max_batch_size = 0;
    /** Rich batch-size constraint */
    BatchMode batch_mode = DynamicBatch{1, 0};
    /** Post-encoder module registry key; empty = none recommended */
    std::string recommended_post_encoder;
};

}// namespace dl

#endif// NEURALYZER_MODEL_INFO_HPP
