#ifndef WHISKERTOOLBOX_V2_PREPROCESSING_REGISTRY_HPP
#define WHISKERTOOLBOX_V2_PREPROCESSING_REGISTRY_HPP

#include <typeindex>
#include <unordered_set>
#include <vector>


namespace WhiskerToolbox::Transforms::V2 {

// ============================================================================
// Preprocessing Registry
// ============================================================================

/**
 * @brief Global registry for parameter types that support preprocessing
 * 
 * This is a simple set that tracks which parameter types have preprocessing capability.
 * Parameter types register themselves at their definition site using RAII.
 * 
 * The registry doesn't store preprocessing logic - it just tracks which types
 * need preprocessing attempts. The actual dispatch uses template instantiation
 * at the call site where types are known.
 */
class PreprocessingRegistry {
public:
    static PreprocessingRegistry & instance() {
        static PreprocessingRegistry registry;
        return registry;
    }

    /**
     * @brief Register that a parameter type supports preprocessing
     */
    void registerType(std::type_index type_id) {
        registered_types_.insert(type_id);
    }

    /**
     * @brief Check if a parameter type is registered for preprocessing
     */
    bool isRegistered(std::type_index type_id) const {
        return registered_types_.find(type_id) != registered_types_.end();
    }

    /**
     * @brief Get all registered type indices
     * 
     * This is used to iterate over all types that might need preprocessing.
     */
    std::vector<std::type_index> getAllRegisteredTypes() const {
        return std::vector<std::type_index>(registered_types_.begin(), registered_types_.end());
    }

private:
    PreprocessingRegistry() = default;
    std::unordered_set<std::type_index> registered_types_;
};

/**
 * @brief RAII helper for registering preprocessing at static initialization
 * 
 * Usage in parameter header file (where Params is fully defined):
 * ```cpp
 * // In ZScoreNormalization.hpp  
 * namespace {
 *     inline auto const register_zscore_preprocessing = 
 *         RegisterPreprocessing<ZScoreNormalizationParams>();
 * }
 * ```
 * 
 * This simply registers the type_index so PipelineStep knows which params
 * types might need preprocessing attempts.
 */
template<typename Params>
class RegisterPreprocessing {
public:
    RegisterPreprocessing() {
        PreprocessingRegistry::instance().registerType(std::type_index(typeid(Params)));
    }
};

}// namespace WhiskerToolbox::Transforms::V2

#endif// WHISKERTOOLBOX_V2_PREPROCESSING_REGISTRY_HPP