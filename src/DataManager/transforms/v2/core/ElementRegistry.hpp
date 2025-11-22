#ifndef WHISKERTOOLBOX_V2_ELEMENT_REGISTRY_HPP
#define WHISKERTOOLBOX_V2_ELEMENT_REGISTRY_HPP

#include "ElementTransform.hpp"
#include "ContainerTraits.hpp"
#include "Observer/Observer_Data.hpp"

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace WhiskerToolbox::Transforms::V2 {

// ============================================================================
// Empty parameter type for stateless transforms
// ============================================================================

/**
 * @brief Empty parameter struct for transforms that don't need configuration
 */
struct NoParams {};

// ============================================================================
// Transform Metadata
// ============================================================================

/**
 * @brief Metadata about a registered transform
 */
struct TransformMetadata {
    std::string name;
    std::string description;
    std::string category;  // "Image Processing", "Geometry", "Statistics", etc.
    
    std::type_index input_type = typeid(void);
    std::type_index output_type = typeid(void);
    std::type_index params_type = typeid(void);
    
    bool is_multi_input = false;
    size_t input_arity = 1;
    std::vector<std::type_index> individual_input_types;  // For multi-input
    
    // For UI generation
    std::string input_type_name;
    std::string output_type_name;
    std::string params_type_name;
    
    // Version and authorship
    std::string version = "1.0";
    std::string author;
    
    // Performance hints
    bool is_expensive = false;  // Hint for parallelization
    bool is_deterministic = true;
    bool supports_cancellation = false;
};

// ============================================================================
// Element Registry
// ============================================================================

/**
 * @brief Registry for element-level transforms
 * 
 * Maintains a compile-time typed registry of transforms that operate on
 * individual elements (Mask2D, Line2D, float, etc.).
 * 
 * Features:
 * - Type-safe registration and lookup
 * - Query by input/output types
 * - Automatic container lifting
 * - Metadata for UI generation
 */
class ElementRegistry {
public:
    ElementRegistry() = default;
    
    // Make registry non-copyable (singleton-like usage)
    ElementRegistry(ElementRegistry const&) = delete;
    ElementRegistry& operator=(ElementRegistry const&) = delete;
    ElementRegistry(ElementRegistry&&) = default;
    ElementRegistry& operator=(ElementRegistry&&) = default;
    
    // ========================================================================
    // Single-Input Transform Registration
    // ========================================================================
    
    /**
     * @brief Register single-input element transform with parameters
     * 
     * @tparam In Input element type (e.g., Mask2D)
     * @tparam Out Output element type (e.g., float)
     * @tparam Params Parameter type (must be default constructible)
     * 
     * @param name Unique transform name
     * @param func Transform function
     * @param metadata Optional metadata for UI
     */
    template<typename In, typename Out, typename Params>
    void registerTransform(
        std::string const& name,
        std::function<Out(In const&, Params const&)> func,
        TransformMetadata metadata = {}) {
        
        // Create typed transform
        auto transform = std::make_shared<TypedTransform<In, Out, Params>>(std::move(func));
        
        // Complete metadata
        metadata.name = name;
        metadata.input_type = std::type_index(typeid(In));
        metadata.output_type = std::type_index(typeid(Out));
        metadata.params_type = std::type_index(typeid(Params));
        metadata.is_multi_input = false;
        metadata.input_arity = 1;
        
        // Store transform (type-erased)
        auto key = std::make_pair(std::type_index(typeid(In)), name);
        transforms_[key] = transform;
        
        // Store metadata
        metadata_[name] = metadata;
        
        // Update type index maps
        input_type_to_names_[std::type_index(typeid(In))].push_back(name);
        output_type_to_names_[std::type_index(typeid(Out))].push_back(name);
    }
    
    /**
     * @brief Register stateless transform (no parameters)
     */
    template<typename In, typename Out>
    void registerTransform(
        std::string const& name,
        std::function<Out(In const&)> func,
        TransformMetadata metadata = {}) {
        
        registerTransform<In, Out, NoParams>(
            name,
            [f = std::move(func)](In const& in, NoParams const&) { return f(in); },
            metadata
        );
    }
    
    // ========================================================================
    // Multi-Input Transform Registration
    // ========================================================================
    
    /**
     * @brief Register binary element transform
     * 
     * Wraps binary function as tuple-input function for consistency.
     */
    template<typename In1, typename In2, typename Out, typename Params>
    void registerBinaryTransform(
        std::string const& name,
        std::function<Out(In1 const&, In2 const&, Params const&)> func,
        TransformMetadata metadata = {}) {
        
        // Wrap as tuple-input function
        auto wrapped = [f = std::move(func)](
            std::tuple<In1, In2> const& inputs,
            Params const& params) -> Out {
            return f(std::get<0>(inputs), std::get<1>(inputs), params);
        };
        
        using TupleIn = std::tuple<In1, In2>;
        auto transform = std::make_shared<TypedTransform<TupleIn, Out, Params>>(wrapped);
        
        // Complete metadata
        metadata.name = name;
        metadata.input_type = std::type_index(typeid(TupleIn));
        metadata.output_type = std::type_index(typeid(Out));
        metadata.params_type = std::type_index(typeid(Params));
        metadata.is_multi_input = true;
        metadata.input_arity = 2;
        metadata.individual_input_types = {
            std::type_index(typeid(In1)),
            std::type_index(typeid(In2))
        };
        
        // Store
        auto key = std::make_pair(std::type_index(typeid(TupleIn)), name);
        transforms_[key] = transform;
        metadata_[name] = metadata;
        
        // Update maps (both individual types point to this transform)
        input_type_to_names_[std::type_index(typeid(In1))].push_back(name);
        input_type_to_names_[std::type_index(typeid(In2))].push_back(name);
        output_type_to_names_[std::type_index(typeid(Out))].push_back(name);
    }
    
    // ========================================================================
    // Transform Execution
    // ========================================================================
    
    /**
     * @brief Execute single-input transform
     * 
     * @throws std::runtime_error if transform not found or type mismatch
     */
    template<typename In, typename Out, typename Params>
    Out execute(std::string const& name,
                In const& input,
                Params const& params,
                ComputeContext const& ctx = {}) const {
        
        auto transform = getTransform<In, Out, Params>(name);
        if (!transform) {
            throw std::runtime_error("Transform not found: " + name);
        }
        
        return transform->execute(input, params, ctx);
    }
    
    /**
     * @brief Execute binary transform
     */
    template<typename In1, typename In2, typename Out, typename Params>
    Out executeBinary(std::string const& name,
                     In1 const& input1,
                     In2 const& input2,
                     Params const& params,
                     ComputeContext const& ctx = {}) const {
        
        using TupleIn = std::tuple<In1, In2>;
        auto transform = getTransform<TupleIn, Out, Params>(name);
        if (!transform) {
            throw std::runtime_error("Transform not found: " + name);
        }
        
        auto inputs = std::tie(input1, input2);
        return transform->execute(inputs, params, ctx);
    }
    
    // ========================================================================
    // Container-Level Execution (Automatic Lifting)
    // ========================================================================
    
    /**
     * @brief Get a callable transform function from the registry
     * 
     * Returns a lambda that captures the transform and parameters,
     * allowing it to be used with standard algorithms or materializers.
     * 
     * @tparam In Input element type
     * @tparam Out Output element type
     * @tparam Params Parameter type
     * 
     * @param name Transform name
     * @param params Parameters to bind to the transform
     * @return Callable that applies the transform
     * 
     * @throws std::runtime_error if transform not found
     */
    template<typename In, typename Out, typename Params>
    auto getTransformFunction(std::string const& name, Params const& params) const {
        auto transform = getTransform<In, Out, Params>(name);
        if (!transform) {
            throw std::runtime_error("Transform not found: " + name);
        }
        
        return [transform, params](In const& input) -> Out {
            return transform->execute(input, params, ComputeContext{});
        };
    }
    
    /**
     * @brief Materialize container transform using registered element transform
     * 
     * Applies the element transform to each element in the input container
     * and materializes the results into an output container.
     * 
     * @tparam ContainerIn Input container type (e.g., MaskData)
     * @tparam ContainerOut Output container type (e.g., RaggedAnalogTimeSeries)
     * @tparam Params Parameter type
     * 
     * @param name Transform name to look up
     * @param input Input container
     * @param params Transform parameters
     * @return Output container by value (caller manages ownership)
     * 
     * @note Does NOT set TimeFrame - caller should handle metadata transfer
     * @note Does NOT set ImageSize - caller should handle metadata transfer
     */
    template<typename ContainerIn, typename ContainerOut, typename Params>
    ContainerOut materializeContainer(
        std::string const& name,
        ContainerIn const& input,
        Params const& params) const;
    
    // ========================================================================
    // Query Interface
    // ========================================================================
    
    /**
     * @brief Get all transform names applicable to an input type
     */
    std::vector<std::string> getTransformsForInputType(std::type_index input_type) const {
        auto it = input_type_to_names_.find(input_type);
        if (it != input_type_to_names_.end()) {
            return it->second;
        }
        return {};
    }
    
    /**
     * @brief Get all transform names that produce an output type
     */
    std::vector<std::string> getTransformsForOutputType(std::type_index output_type) const {
        auto it = output_type_to_names_.find(output_type);
        if (it != output_type_to_names_.end()) {
            return it->second;
        }
        return {};
    }
    
    /**
     * @brief Get all transform names that take In and produce Out
     */
    template<typename In, typename Out>
    std::vector<std::string> getCompatibleTransforms() const {
        std::vector<std::string> result;
        
        auto input_transforms = getTransformsForInputType(typeid(In));
        
        for (auto const& name : input_transforms) {
            auto const& meta = metadata_.at(name);
            if (meta.output_type == typeid(Out)) {
                result.push_back(name);
            }
        }
        
        return result;
    }
    
    /**
     * @brief Get metadata for a transform
     */
    TransformMetadata const* getMetadata(std::string const& name) const {
        auto it = metadata_.find(name);
        return (it != metadata_.end()) ? &it->second : nullptr;
    }
    
    /**
     * @brief Check if transform exists
     */
    bool hasTransform(std::string const& name) const {
        return metadata_.find(name) != metadata_.end();
    }
    
    /**
     * @brief Get all registered transform names
     */
    std::vector<std::string> getAllTransformNames() const {
        std::vector<std::string> names;
        names.reserve(metadata_.size());
        for (auto const& [name, meta] : metadata_) {
            names.push_back(name);
        }
        return names;
    }

private:
    /**
     * @brief Internal: Get typed transform
     */
    template<typename In, typename Out, typename Params>
    std::shared_ptr<TypedTransform<In, Out, Params>> 
    getTransform(std::string const& name) const {
        auto key = std::make_pair(std::type_index(typeid(In)), name);
        auto it = transforms_.find(key);
        
        if (it == transforms_.end()) {
            return nullptr;
        }
        
        return std::static_pointer_cast<TypedTransform<In, Out, Params>>(it->second);
    }
    
    // Hash function for pair<type_index, string>
    struct PairHash {
        template<typename T1, typename T2>
        std::size_t operator()(std::pair<T1, T2> const& p) const {
            auto h1 = std::hash<T1>{}(p.first);
            auto h2 = std::hash<T2>{}(p.second);
            return h1 ^ (h2 << 1);
        }
    };
    
    // Storage
    std::unordered_map<
        std::pair<std::type_index, std::string>,
        std::shared_ptr<void>,
        PairHash
    > transforms_;
    
    std::unordered_map<std::string, TransformMetadata> metadata_;
    
    std::unordered_map<std::type_index, std::vector<std::string>> input_type_to_names_;
    std::unordered_map<std::type_index, std::vector<std::string>> output_type_to_names_;
};

// ============================================================================
// Container Transform Implementation
// ============================================================================

/**
 * @brief Materialize container transform for RaggedAnalogTimeSeries output
 * 
 * Simplified design:
 * - Returns by value (caller manages ownership)
 * - Does NOT set TimeFrame (caller's responsibility)
 * - Does NOT set ImageSize (caller's responsibility)
 * - Focuses purely on data transformation
 */
template<typename ContainerIn, typename ContainerOut, typename Params>
ContainerOut ElementRegistry::materializeContainer(
    std::string const& name,
    ContainerIn const& input,
    Params const& params) const {
    
    using In = ElementFor_t<ContainerIn>;
    using Out = ElementFor_t<ContainerOut>;
    
    // Get transform function
    auto transform_fn = getTransformFunction<In, Out, Params>(name, params);
    
    // Create output container (by value)
    ContainerOut output;
    
    // Apply transform to each element and accumulate results
    for (auto [time, entry] : input.elements()) {
        Out result = transform_fn(entry.data);
        
        // Append to output container
        if constexpr (std::is_same_v<ContainerOut, RaggedAnalogTimeSeries>) {
            output.appendAtTime(time, {result}, NotifyObservers::No);
        }
        else if constexpr (RaggedContainer<ContainerOut>) {
            output.addAtTime(time, result, NotifyObservers::No);
        }
        else {
            output.setAtTime(time, result, NotifyObservers::No);
        }
    }
    
    return output;
}

} // namespace WhiskerToolbox::Transforms::V2

#endif // WHISKERTOOLBOX_V2_ELEMENT_REGISTRY_HPP
