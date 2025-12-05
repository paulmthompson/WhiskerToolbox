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
// Container Executor Interface
// ============================================================================

/**
 * @brief Interface for type-erased container execution
 * 
 * This interface allows executing container transforms without knowing
 * the concrete input/output/parameter types at the call site. The types
 * are captured at registration time.
 * 
 * This mirrors IParamExecutor for element transforms, enabling container
 * transforms to be first-class citizens in the pipeline system.
 */
class IContainerExecutor {
public:
    virtual ~IContainerExecutor() = default;
    
    /**
     * @brief Execute the container transform on type-erased input
     * 
     * @param name Transform name (for registry lookup)
     * @param input_variant Input data as DataTypeVariant
     * @param ctx Compute context for progress/cancellation
     * @return Output data as DataTypeVariant
     * 
     * @throws std::runtime_error if input type doesn't match expected type
     */
    virtual DataTypeVariant execute(
            std::string const & name,
            DataTypeVariant const & input_variant,
            ComputeContext const & ctx) const = 0;
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
 * Inherits from IContainerExecutor to provide type-erased execution,
 * enabling container transforms to be executed dynamically without
 * knowing concrete types at the call site.
 * 
 * Note: Implementation is defined after ElementRegistry class declaration.
 */
template<typename InContainer, typename OutContainer, typename Params>
class TypedContainerExecutor : public IContainerExecutor {
public:
    explicit TypedContainerExecutor(Params params);

    /**
     * @brief Execute with concrete types (for direct typed calls)
     */
    std::shared_ptr<OutContainer> executeTyped(
            std::string const & name,
            InContainer const & input,
            ComputeContext const & ctx) const;

    /**
     * @brief Type-erased execution via IContainerExecutor interface
     * 
     * Extracts the concrete input type from the variant, executes,
     * and wraps the result back in a DataTypeVariant.
     */
    DataTypeVariant execute(
            std::string const & name,
            DataTypeVariant const & input_variant,
            ComputeContext const & ctx) const override;

private:
    Params params_;
};

}// namespace WhiskerToolbox::Transforms::V2


#endif// WHISKERTOOLBOX_V2_CONTAINER_REGISTRY_HPP