#ifndef WHISKERTOOLBOX_V2_CONTAINER_REGISTRY_HPP
#define WHISKERTOOLBOX_V2_CONTAINER_REGISTRY_HPP

#include <string>
#include <typeindex>
#include <vector>

namespace WhiskerToolbox::Transforms::V2 {

/**
 * @brief Metadata specific to container-level transforms
 * 
 * Separate from TransformMetadata to avoid polluting element transforms
 * with unnecessary flags.
 */
struct ContainerTransformMetadata {
    std::string name;
    std::string description;
    std::string category;// "Signal Processing", "Time Series Analysis", etc.

    std::type_index input_container_type = typeid(void);
    std::type_index output_container_type = typeid(void);
    std::type_index params_type = typeid(void);

    // Multi-input support (mirrors TransformMetadata)
    bool is_multi_input = false;
    size_t input_arity = 1;
    std::vector<std::type_index> individual_input_types;// For multi-input

    // For UI generation
    std::string input_type_name;
    std::string output_type_name;
    std::string params_type_name;

    // Version and authorship
    std::string version = "1.0";
    std::string author;

    // Performance hints
    bool is_expensive = false;// Hint for showing progress UI
    bool is_deterministic = true;
    bool supports_cancellation = true;// Most container transforms support this
};


}// namespace WhiskerToolbox::Transforms::V2


#endif// WHISKERTOOLBOX_V2_CONTAINER_REGISTRY_HPP