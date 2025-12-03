#ifndef WHISKERTOOLBOX_V2_CONTAINER_REGISTRY_HPP
#define WHISKERTOOLBOX_V2_CONTAINER_REGISTRY_HPP

#include "ComputeContext.hpp"
#include "DataManagerTypes.hpp"// DataTypeVariant

#include <functional>
#include <memory>
#include <string>
#include <typeindex>

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


// ============================================================================
// Container Transform Infrastructure
// ============================================================================

/**
 * @brief Type-erased wrapper for container transforms
 * 
 * This is INTERNAL to the registry - transforms themselves don't inherit.
 * Stores the pure function that operates on concrete container types.
 */
template<typename InContainer, typename OutContainer, typename Params>
class TypedContainerTransform {
public:
    using FuncType = std::function<std::shared_ptr<OutContainer>(
            InContainer const &,
            Params const &,
            ComputeContext const &)>;

    explicit TypedContainerTransform(FuncType func)
        : func_(std::move(func)) {}

    std::shared_ptr<OutContainer> execute(
            InContainer const & input,
            Params const & params,
            ComputeContext const & ctx) const {
        return func_(input, params, ctx);
    }

private:
    FuncType func_;
};

/**
 * @brief Executor for container transforms with captured parameters
 * 
 * Similar to TypedParamExecutor but for container-level operations.
 * Eliminates per-call parameter casts by capturing params at construction.
 * 
 * Note: Implementation is defined after ElementRegistry class declaration.
 */
template<typename InContainer, typename OutContainer, typename Params>
class TypedContainerExecutor {
public:
    explicit TypedContainerExecutor(Params params);

    std::shared_ptr<OutContainer> execute(
            std::string const & name,
            InContainer const & input,
            ComputeContext const & ctx) const;

    // Type-erased version for pipeline (converts DataTypeVariant)
    DataTypeVariant executeVariant(
            std::string const & name,
            DataTypeVariant const & input_variant,
            ComputeContext const & ctx) const;

private:
    Params params_;
};

}// namespace WhiskerToolbox::Transforms::V2


#endif// WHISKERTOOLBOX_V2_CONTAINER_REGISTRY_HPP