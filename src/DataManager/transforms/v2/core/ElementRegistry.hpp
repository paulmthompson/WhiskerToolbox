#ifndef WHISKERTOOLBOX_V2_ELEMENT_REGISTRY_HPP
#define WHISKERTOOLBOX_V2_ELEMENT_REGISTRY_HPP

#include "ElementTransform.hpp"
#include "ContainerTraits.hpp"
#include "Observer/Observer_Data.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <any>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <utility>
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
// Type Triple for Parameter Executor Lookup
// ============================================================================

/**
 * @brief Key for looking up typed executors by full type signature
 */
struct TypeTriple {
    std::type_index input_type;
    std::type_index output_type;
    std::type_index params_type;
    
    bool operator==(TypeTriple const& other) const {
        return input_type == other.input_type &&
               output_type == other.output_type &&
               params_type == other.params_type;
    }
};

/**
 * @brief Hash function for TypeTriple
 */
struct TypeTripleHash {
    std::size_t operator()(TypeTriple const& key) const {
        std::size_t h1 = std::hash<std::type_index>{}(key.input_type);
        std::size_t h2 = std::hash<std::type_index>{}(key.output_type);
        std::size_t h3 = std::hash<std::type_index>{}(key.params_type);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

// ============================================================================
// Typed Parameter Executor (stores parameters and types)
// ============================================================================

/**
 * @brief Interface for parameter executors with captured state
 * 
 * Each executor knows its input/output types and has parameters captured.
 * This eliminates per-element casts and type dispatch.
 */
class IParamExecutor {
public:
    virtual ~IParamExecutor() = default;
    virtual std::any execute(std::string const& name, std::any const& input) const = 0;
};

/**
 * @brief Interface for time-grouped parameter executors with captured state
 */
class ITimeGroupedParamExecutor {
public:
    virtual ~ITimeGroupedParamExecutor() = default;
    virtual std::any execute(std::string const& name, std::any const& input_span) const = 0;
};

/**
 * @brief Concrete executor with full type information and captured parameters
 * 
 * All types are known at construction time, eliminating runtime dispatch.
 * Parameters are captured, eliminating per-element casts.
 * 
 * Note: Implementation is defined after ElementRegistry class declaration.
 */
template<typename In, typename Out, typename Params>
class TypedParamExecutor : public IParamExecutor {
public:
    explicit TypedParamExecutor(Params params);
    
    std::any execute(std::string const& name, std::any const& input_any) const override;
    
private:
    Params params_;  // Parameters captured at construction
};

/**
 * @brief Concrete time-grouped executor with full type information and captured parameters
 */
template<typename In, typename Out, typename Params>
class TypedTimeGroupedParamExecutor : public ITimeGroupedParamExecutor {
public:
    explicit TypedTimeGroupedParamExecutor(Params params);
    
    std::any execute(std::string const& name, std::any const& input_span_any) const override;
    
private:
    Params params_;
};

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
    
    bool is_time_grouped = false;  // True if this operates on span<Element> per time
    
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
    
    /**
     * @brief Get global singleton instance
     * 
     * @return Reference to the global registry
     */
    static ElementRegistry& instance() {
        static ElementRegistry registry;
        return registry;
    }
    
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
        
        // Auto-register all parameter-related factories (JSON, validator, executor, PipelineStep)
        registerParamDeserializerIfNeeded<In, Out, Params>();
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
    // Time-Grouped Transform Registration (M→N per time)
    // ========================================================================
    
    /**
     * @brief Register time-grouped transform
     * 
     * For transforms that operate on all elements at a single time point.
     * Signature: (span<In>, Params) → vector<Out>
     * 
     * @tparam In Input element type (e.g., float)
     * @tparam Out Output element type (e.g., float)
     * @tparam Params Parameter type
     * 
     * Example: Sum reduction (span<float> → vector<float> with one element)
     */
    template<typename In, typename Out, typename Params>
    void registerTimeGroupedTransform(
        std::string const& name,
        std::function<std::vector<Out>(std::span<In const>, Params const&)> func,
        TransformMetadata metadata = {}) {
        
        // Create typed transform
        auto transform = std::make_shared<TypedTimeGroupedTransform<In, Out, Params>>(std::move(func));
        
        // Complete metadata
        metadata.name = name;
        metadata.input_type = std::type_index(typeid(In));
        metadata.output_type = std::type_index(typeid(Out));
        metadata.params_type = std::type_index(typeid(Params));
        metadata.is_multi_input = false;
        metadata.input_arity = 1;
        metadata.is_time_grouped = true;
        
        // Store transform (type-erased)
        auto key = std::make_pair(std::type_index(typeid(In)), name);
        time_grouped_transforms_[key] = transform;
        
        // Store metadata
        metadata_[name] = metadata;
        
        // Update type index maps
        input_type_to_names_[std::type_index(typeid(In))].push_back(name);
        output_type_to_names_[std::type_index(typeid(Out))].push_back(name);
        
        // Auto-register all parameter-related factories
        registerParamDeserializerIfNeeded<In, Out, Params>();
    }
    
    /**
     * @brief Register stateless time-grouped transform (no parameters)
     */
    template<typename In, typename Out>
    void registerTimeGroupedTransform(
        std::string const& name,
        std::function<std::vector<Out>(std::span<In const>)> func,
        TransformMetadata metadata = {}) {
        
        registerTimeGroupedTransform<In, Out, NoParams>(
            name,
            [f = std::move(func)](std::span<In const> in, NoParams const&) { return f(in); },
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
        
        // Auto-register all parameter-related factories
        // For binary transforms, use the tuple input type
        registerParamDeserializerIfNeeded<TupleIn, Out, Params>();
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
    
    /**
     * @brief Execute time-grouped transform
     * 
     * @throws std::runtime_error if transform not found or type mismatch
     */
    template<typename In, typename Out, typename Params>
    std::vector<Out> executeTimeGrouped(std::string const& name,
                                       std::span<In const> inputs,
                                       Params const& params) const {
        
        auto transform = getTimeGroupedTransform<In, Out, Params>(name);
        if (!transform) {
            throw std::runtime_error("Time-grouped transform not found: " + name);
        }
        
        return transform->execute(inputs, params);
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
        for (auto const& [name, _] : metadata_) {
            names.push_back(name);
        }
        return names;
    }
    
    /**
     * @brief Register JSON parameter deserializer for a parameter type
     * 
     * This allows the registry to deserialize JSON strings into typed parameters
     * without needing a variant or manual dispatch.
     * 
     * @tparam Params Parameter type
     * @param deserializer Function that converts JSON string to std::any containing Params
     */
    template<typename Params>
    void registerParamDeserializer(
        std::function<std::any(std::string const&)> deserializer)
    {
        param_deserializers_[typeid(Params)] = std::move(deserializer);
    }
    
    /**
     * @brief Deserialize JSON parameters for a transform by name
     * 
     * Looks up the transform's parameter type and uses the registered deserializer.
     * 
     * @param transform_name Name of the transform
     * @param json_str JSON string containing parameters
     * @return std::any containing typed parameters, or empty std::any on failure
     */
    std::any deserializeParameters(
        std::string const& transform_name,
        std::string const& json_str) const
    {
        // Get metadata to find parameter type
        auto const* metadata = getMetadata(transform_name);
        if (!metadata) {
            return std::any{}; // Transform not found
        }
        
        // Look up deserializer for this parameter type
        auto it = param_deserializers_.find(metadata->params_type);
        if (it == param_deserializers_.end()) {
            return std::any{}; // No deserializer registered
        }
        
        // Deserialize
        try {
            return it->second(json_str);
        } catch (...) {
            return std::any{}; // Deserialization failed
        }
    }
    
    /**
     * @brief Validate that parameters match the expected type for a transform
     * 
     * This checks that the std::any contains parameters of the correct type
     * for the given transform, based on registered metadata.
     * 
     * @param transform_name Name of the transform
     * @param params_any Type-erased parameters to validate
     * @return true if parameters are valid for this transform
     */
    bool validateParameters(
        std::string const& transform_name,
        std::any const& params_any) const
    {
        // Get metadata to find parameter type
        auto const* metadata = getMetadata(transform_name);
        if (!metadata) {
            return false; // Transform not found
        }
        
        // Look up validator for this parameter type
        auto it = param_validators_.find(metadata->params_type);
        if (it == param_validators_.end()) {
            return false; // No validator registered
        }
        
        // Validate
        return it->second(params_any);
    }
    
    // ========================================================================
    // Typed Parameter Executors (eliminates per-element dispatch)
    // ========================================================================
    
    /**
     * @brief Register a typed executor factory for a specific type signature
     * 
     * The factory creates executors with parameters already captured,
     * eliminating per-element parameter casts and type dispatch.
     * 
     * @tparam In Input element type
     * @tparam Out Output element type
     * @tparam Params Parameter type
     * @param factory Function that creates executor from type-erased params
     */
    template<typename In, typename Out, typename Params>
    void registerTypedExecutorFactory(
        std::function<std::unique_ptr<IParamExecutor>(std::any const&)> factory)
    {
        TypeTriple key{typeid(In), typeid(Out), typeid(Params)};
        typed_executor_factories_[key] = std::move(factory);
    }
    
    /**
     * @brief Register a typed time-grouped executor factory
     */
    template<typename In, typename Out, typename Params>
    void registerTimeGroupedExecutorFactory(
        std::function<std::unique_ptr<ITimeGroupedParamExecutor>(std::any const&)> factory)
    {
        TypeTriple key{typeid(In), typeid(Out), typeid(Params)};
        time_grouped_executor_factories_[key] = std::move(factory);
    }

    /**
     * @brief Get or create a typed executor for given parameters
     * 
     * This looks up the factory, creates an executor with parameters captured,
     * and caches it for reuse.
     * 
     * @param key Type triple identifying the transform signature
     * @param params Type-erased parameters
     * @return Pointer to executor with captured state
     */
    IParamExecutor const* getOrCreateTypedExecutor(
        TypeTriple const& key,
        std::any const& params) const
    {
        // Check cache first
        auto cache_key = std::make_pair(key, std::type_index(params.type()));
        auto cache_it = typed_executor_cache_.find(cache_key);
        if (cache_it != typed_executor_cache_.end()) {
            return cache_it->second.get();
        }
        
        // Look up factory
        auto factory_it = typed_executor_factories_.find(key);
        if (factory_it == typed_executor_factories_.end()) {
            throw std::runtime_error("No typed executor factory registered for type triple: " +
                                   std::string(key.input_type.name()) + " -> " +
                                   std::string(key.output_type.name()) + " (" +
                                   std::string(key.params_type.name()) + ")");
        }
        
        // Create executor with captured parameters
        auto executor = factory_it->second(params);
        auto* executor_ptr = executor.get();
        
        // Cache for reuse
        typed_executor_cache_[cache_key] = std::move(executor);
        
        return executor_ptr;
    }
    
    /**
     * @brief Get or create a typed time-grouped executor for given parameters
     */
    ITimeGroupedParamExecutor const* getOrCreateTimeGroupedExecutor(
        TypeTriple const& key,
        std::any const& params) const
    {
        // Check cache first
        auto cache_key = std::make_pair(key, std::type_index(params.type()));
        auto cache_it = time_grouped_executor_cache_.find(cache_key);
        if (cache_it != time_grouped_executor_cache_.end()) {
            return cache_it->second.get();
        }
        
        // Look up factory
        auto factory_it = time_grouped_executor_factories_.find(key);
        if (factory_it == time_grouped_executor_factories_.end()) {
            throw std::runtime_error("No typed time-grouped executor factory registered for type triple: " +
                                   std::string(key.input_type.name()) + " -> " +
                                   std::string(key.output_type.name()) + " (" +
                                   std::string(key.params_type.name()) + ")");
        }
        
        // Create executor with captured parameters
        auto executor = factory_it->second(params);
        auto* executor_ptr = executor.get();
        
        // Cache for reuse
        time_grouped_executor_cache_[cache_key] = std::move(executor);
        
        return executor_ptr;
    }

    /**
     * @brief Legacy registration for backward compatibility
     * 
     * @deprecated Use registerTypedExecutorFactory instead
     */
    template<typename Params>
    void registerParamExecutor(
        std::function<std::any(
            std::string const& transform_name,
            std::any const& input_element,
            std::any const& params,
            std::type_index in_type,
            std::type_index out_type)> executor)
    {
        param_executors_[typeid(Params)] = std::move(executor);
    }
    
    /**
     * @brief Execute transform with typed executor (zero per-element dispatch)
     * 
     * Looks up pre-built executor with captured parameters and types,
     * eliminating all per-element casts and dispatch overhead.
     * 
     * @param transform_name Name of the transform
     * @param input_element Input element (type-erased)
     * @param params Parameters (type-erased)
     * @param in_type Input element type
     * @param out_type Output element type
     * @param param_type Parameter type
     * @return Output element (type-erased)
     */
    std::any executeWithDynamicParams(
        std::string const& transform_name,
        std::any const& input_element,
        std::any const& params,
        std::type_index in_type,
        std::type_index out_type,
        std::type_index param_type) const
    {
        TypeTriple key{in_type, out_type, param_type};
        
        // Get or create executor with captured state
        auto const* executor = getOrCreateTypedExecutor(key, params);
        
        // Execute with zero dispatch overhead!
        return executor->execute(transform_name, input_element);
    }
    
    /**
     * @brief Execute time-grouped transform with typed executor
     */
    std::any executeTimeGroupedWithDynamicParams(
        std::string const& transform_name,
        std::any const& input_span,
        std::any const& params,
        std::type_index in_type,
        std::type_index out_type,
        std::type_index param_type) const
    {
        TypeTriple key{in_type, out_type, param_type};
        
        // Get or create executor with captured state
        auto const* executor = getOrCreateTimeGroupedExecutor(key, params);
        
        // Execute
        return executor->execute(transform_name, input_span);
    }

    /**
     * @brief Legacy executeWithDynamicParams for backward compatibility
     * 
     * Falls back to old param_executors_ map if typed executor not found.
     * 
     * @deprecated Typed executors should be registered instead
     */
    std::any executeWithDynamicParamsLegacy(
        std::string const& transform_name,
        std::any const& input_element,
        std::any const& params,
        std::type_index in_type,
        std::type_index out_type,
        std::type_index param_type) const
    {
        auto it = param_executors_.find(param_type);
        if (it == param_executors_.end()) {
            throw std::runtime_error("No executor registered for parameter type: " + 
                                    std::string(param_type.name()));
        }
        
        return it->second(transform_name, input_element, params, in_type, out_type);
    }

private:
    /**
     * @brief Auto-register all parameter-related factories for a parameter type
     * 
     * This is called automatically during transform registration and sets up:
     * 1. JSON deserializer (using reflect-cpp)
     * 2. Parameter validator
     * 3. Typed executor factory (for pipeline execution)
     * 4. PipelineStep factory (for pipeline loading from JSON)
     * 
     * @tparam In Input element type
     * @tparam Out Output element type
     * @tparam Params Parameter type (must be reflect-cpp serializable)
     */
    template<typename In, typename Out, typename Params>
    void registerParamDeserializerIfNeeded() {
        auto type_idx = std::type_index(typeid(Params));
        
        // Only register deserializer/validator if not already present (per Params type)
        if (param_deserializers_.find(type_idx) == param_deserializers_.end()) {
            // 1. Register JSON deserializer using reflect-cpp
            param_deserializers_[type_idx] = [](std::string const& json_str) -> std::any {
                auto result = rfl::json::read<Params>(json_str);
                if (result) {
                    return std::any{result.value()};
                }
                return std::any{}; // Failed deserialization
            };
            
            // 2. Register validator to check std::any contains correct type
            param_validators_[type_idx] = [](std::any const& params_any) -> bool {
                try {
                    std::any_cast<Params const&>(params_any);
                    return true;
                } catch (...) {
                    return false;
                }
            };
        }
        
        // ALWAYS register executor factories for this specific (In, Out, Params) combination
        // These are keyed by the triple, so we need to register them even if Params is already known
        
        // 3. Register typed executor factory (for zero-dispatch pipeline execution)
        registerTypedExecutorFactory<In, Out, Params>(
            [](std::any const& params_any) -> std::unique_ptr<IParamExecutor> {
                auto params = std::any_cast<Params>(params_any);
                return std::make_unique<TypedParamExecutor<In, Out, Params>>(
                    std::move(params));
            });

        // 4. Register typed time-grouped executor factory
        registerTimeGroupedExecutorFactory<In, Out, Params>(
            [](std::any const& params_any) -> std::unique_ptr<ITimeGroupedParamExecutor> {
                auto params = std::any_cast<Params>(params_any);
                return std::make_unique<TypedTimeGroupedParamExecutor<In, Out, Params>>(
                    std::move(params));
            });
    }
    
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
    
    /**
     * @brief Internal: Get typed time-grouped transform
     */
    template<typename In, typename Out, typename Params>
    std::shared_ptr<TypedTimeGroupedTransform<In, Out, Params>> 
    getTimeGroupedTransform(std::string const& name) const {
        auto key = std::make_pair(std::type_index(typeid(In)), name);
        auto it = time_grouped_transforms_.find(key);
        
        if (it == time_grouped_transforms_.end()) {
            return nullptr;
        }
        
        return std::static_pointer_cast<TypedTimeGroupedTransform<In, Out, Params>>(it->second);
    }
    
    // Hash function for pair<type_index, string>
    struct PairHash {
        template<typename T1, typename T2>
        std::size_t operator()(std::pair<T1, T2> const& p) const {
            auto h1 = std::hash<T1>{}(p.first);
            auto h2 = std::hash<T2>{}(p.second);
            return h1 ^ (h2 << 1);
        }
        
        // Specialized hash for pair<TypeTriple, std::type_index>
        std::size_t operator()(std::pair<TypeTriple, std::type_index> const& p) const {
            TypeTripleHash triple_hash;
            auto h1 = triple_hash(p.first);
            auto h2 = std::hash<std::type_index>{}(p.second);
            return h1 ^ (h2 << 1);
        }
    };
    
    // Storage
    std::unordered_map<
        std::pair<std::type_index, std::string>,
        std::shared_ptr<void>,
        PairHash
    > transforms_;
    
    std::unordered_map<
        std::pair<std::type_index, std::string>,
        std::shared_ptr<void>,
        PairHash
    > time_grouped_transforms_;
    
    std::unordered_map<std::string, TransformMetadata> metadata_;
    
    std::unordered_map<std::type_index, std::vector<std::string>> input_type_to_names_;
    std::unordered_map<std::type_index, std::vector<std::string>> output_type_to_names_;
    
    // Typed executor factories (create executors with captured parameters)
    std::unordered_map<
        TypeTriple,
        std::function<std::unique_ptr<IParamExecutor>(std::any const&)>,
        TypeTripleHash
    > typed_executor_factories_;
    
    std::unordered_map<
        TypeTriple,
        std::function<std::unique_ptr<ITimeGroupedParamExecutor>(std::any const&)>,
        TypeTripleHash
    > time_grouped_executor_factories_;

    // Cache of created executors (reused across pipeline executions)
    // Key: (TypeTriple, param_type_index) -> unique ownership of executor
    mutable std::unordered_map<
        std::pair<TypeTriple, std::type_index>,
        std::unique_ptr<IParamExecutor>,
        PairHash
    > typed_executor_cache_;
    
    mutable std::unordered_map<
        std::pair<TypeTriple, std::type_index>,
        std::unique_ptr<ITimeGroupedParamExecutor>,
        PairHash
    > time_grouped_executor_cache_;

    // Legacy parameter type executors (for backward compatibility)
    std::unordered_map<
        std::type_index,
        std::function<std::any(std::string const&, std::any const&, std::any const&, 
                              std::type_index, std::type_index)>
    > param_executors_;
    
    // JSON parameter deserializers (type_index -> JSON string -> std::any)
    std::unordered_map<
        std::type_index,
        std::function<std::any(std::string const&)>
    > param_deserializers_;
    
    // Parameter type validators (type_index -> std::any validator)
    // Just validates that std::any contains the correct type
    mutable std::unordered_map<
        std::type_index,
        std::function<bool(std::any const&)>
    > param_validators_;
};

// ============================================================================
// TypedParamExecutor Implementation (after ElementRegistry is fully declared)
// ============================================================================

template<typename In, typename Out, typename Params>
TypedParamExecutor<In, Out, Params>::TypedParamExecutor(Params params)
    : params_(std::move(params))
{}

template<typename In, typename Out, typename Params>
std::any TypedParamExecutor<In, Out, Params>::execute(
    std::string const& name,
    std::any const& input_any) const
{
    // All types known - zero dispatch cost!
    auto const& input = std::any_cast<In const&>(input_any);
    auto& registry = ElementRegistry::instance();
    Out result = registry.execute<In, Out, Params>(name, input, params_);
    return std::any{result};
}

template<typename In, typename Out, typename Params>
TypedTimeGroupedParamExecutor<In, Out, Params>::TypedTimeGroupedParamExecutor(Params params)
    : params_(std::move(params))
{}

template<typename In, typename Out, typename Params>
std::any TypedTimeGroupedParamExecutor<In, Out, Params>::execute(
    std::string const& name,
    std::any const& input_span_any) const
{
    auto const& input_span = std::any_cast<std::span<In const> const&>(input_span_any);
    auto& registry = ElementRegistry::instance();
    std::vector<Out> result = registry.executeTimeGrouped<In, Out, Params>(name, input_span, params_);
    return std::any{result};
}

// ============================================================================
// Helper: Auto-register typed executor factory during transform registration
// ============================================================================

/**
 * @brief Automatically register typed executor factory when registering transform
 * 
 * This helper ensures that every registered transform automatically gets
 * a typed executor factory, eliminating manual registration boilerplate.
 */
template<typename In, typename Out, typename Params>
struct AutoRegisterTypedExecutor {
    AutoRegisterTypedExecutor() {
        auto& registry = ElementRegistry::instance();
        
        registry.registerTypedExecutorFactory<In, Out, Params>(
            [](std::any const& params_any) -> std::unique_ptr<IParamExecutor> {
                auto params = std::any_cast<Params>(params_any);
                return std::make_unique<TypedParamExecutor<In, Out, Params>>(std::move(params));
            });
    }
};

// Invoke during static initialization for each transform type
#define AUTO_REGISTER_TYPED_EXECUTOR(In, Out, Params) \
    namespace { \
        static AutoRegisterTypedExecutor<In, Out, Params> \
            auto_register_typed_executor_##In##_##Out##_##Params; \
    }

// ============================================================================
// Compile-Time Registration Helper
// ============================================================================

/**
 * @brief RAII helper for compile-time transform registration
 * 
 * This class is designed to be instantiated as a static variable,
 * triggering registration during static initialization.
 * 
 * Example usage:
 * ```cpp
 * namespace {
 *     auto const register_mask_area = RegisterTransform<Mask2D, float, MaskAreaParams>(
 *         "CalculateMaskArea",
 *         calculateMaskArea,
 *         TransformMetadata{
 *             .description = "Calculate mask area",
 *             .category = "Image Processing"
 *         }
 *     );
 * }
 * ```
 */
template<typename In, typename Out, typename Params>
class RegisterTransform {
public:
    RegisterTransform(
        std::string const& name,
        std::function<Out(In const&, Params const&)> func,
        TransformMetadata metadata = {})
    {
        ElementRegistry::instance().registerTransform<In, Out, Params>(
            name, std::move(func), std::move(metadata));
    }
};

/**
 * @brief RAII helper for compile-time stateless transform registration
 */
template<typename In, typename Out>
class RegisterStatelessTransform {
public:
    RegisterStatelessTransform(
        std::string const& name,
        std::function<Out(In const&)> func,
        TransformMetadata metadata = {})
    {
        ElementRegistry::instance().registerTransform<In, Out>(
            name, std::move(func), std::move(metadata));
    }
};

/**
 * @brief RAII helper for compile-time context-aware transform registration
 */
template<typename In, typename Out, typename Params>
class RegisterContextTransform {
public:
    RegisterContextTransform(
        std::string const& name,
        std::function<Out(In const&, Params const&, ComputeContext const&)> func,
        TransformMetadata metadata = {})
    {
        // Wrap the context-aware function to match the expected signature
        auto wrapped = [func](In const& in, Params const& p) -> Out {
            ComputeContext ctx;  // Default context
            return func(in, p, ctx);
        };
        ElementRegistry::instance().registerTransform<In, Out, Params>(
            name, wrapped, std::move(metadata));
    }
};

/**
 * @brief RAII helper for compile-time time-grouped transform registration
 */
template<typename In, typename Out, typename Params>
class RegisterTimeGroupedTransform {
public:
    RegisterTimeGroupedTransform(
        std::string const& name,
        std::function<std::vector<Out>(std::span<In const>, Params const&)> func,
        TransformMetadata metadata = {})
    {
        ElementRegistry::instance().registerTimeGroupedTransform<In, Out, Params>(
            name, std::move(func), std::move(metadata));
    }
};

/**
 * @brief RAII helper for compile-time stateless time-grouped transform registration
 */
template<typename In, typename Out>
class RegisterStatelessTimeGroupedTransform {
public:
    RegisterStatelessTimeGroupedTransform(
        std::string const& name,
        std::function<std::vector<Out>(std::span<In const>)> func,
        TransformMetadata metadata = {})
    {
        ElementRegistry::instance().registerTimeGroupedTransform<In, Out>(
            name, std::move(func), std::move(metadata));
    }
};

/**
 * @brief RAII helper for compile-time context-aware time-grouped transform registration
 */
template<typename In, typename Out, typename Params>
class RegisterContextTimeGroupedTransform {
public:
    RegisterContextTimeGroupedTransform(
        std::string const& name,
        std::function<std::vector<Out>(std::span<In const>, Params const&, ComputeContext const&)> func,
        TransformMetadata metadata = {})
    {
        // Wrap the context-aware function to match the expected signature
        auto wrapped = [func](std::span<In const> inputs, Params const& p) -> std::vector<Out> {
            ComputeContext ctx;  // Default context
            return func(inputs, p, ctx);
        };
        ElementRegistry::instance().registerTimeGroupedTransform<In, Out, Params>(
            name, wrapped, std::move(metadata));
    }
};

/**
 * @brief RAII helper for compile-time binary transform registration
 * 
 * Registers a transform that takes two separate inputs.
 */
template<typename In1, typename In2, typename Out, typename Params>
class RegisterBinaryTransform {
public:
    RegisterBinaryTransform(
        std::string const& name,
        std::function<Out(In1 const&, In2 const&, Params const&)> func,
        TransformMetadata metadata = {})
    {
        ElementRegistry::instance().registerBinaryTransform<In1, In2, Out, Params>(
            name, std::move(func), std::move(metadata));
    }
};
} // namespace WhiskerToolbox::Transforms::V2

#endif // WHISKERTOOLBOX_V2_ELEMENT_REGISTRY_HPP
