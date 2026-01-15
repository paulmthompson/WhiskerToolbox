#ifndef WHISKERTOOLBOX_V2_RANGE_REDUCTION_REGISTRY_HPP
#define WHISKERTOOLBOX_V2_RANGE_REDUCTION_REGISTRY_HPP

/**
 * @file RangeReductionRegistry.hpp
 * @brief Registry for range reduction operations
 *
 * This file provides the infrastructure for registering and executing
 * range reductions. Range reductions collapse an entire range of elements
 * into a scalar value (e.g., for sorting trials by spike count).
 *
 * ## Design Goals
 *
 * 1. **Runtime discovery**: Users can query available reductions by input type
 * 2. **Type-erased execution**: Pipeline can execute without knowing concrete types
 * 3. **JSON serialization**: Parameters can be serialized for pipeline persistence
 * 4. **Consistent patterns**: Follows ElementRegistry patterns for familiarity
 *
 * ## Usage
 *
 * ```cpp
 * // Registration (typically via RegisterRangeReduction helper)
 * RangeReductionRegistry::instance().registerReduction<EventWithId, float, FirstLatencyParams>(
 *     "FirstPositiveLatency",
 *     [](std::span<EventWithId const> events, FirstLatencyParams const& params) -> float {
 *         for (auto const& e : events) {
 *             if (e.time() > params.zero_time) return e.time() - params.zero_time;
 *         }
 *         return std::numeric_limits<float>::quiet_NaN();
 *     },
 *     metadata);
 *
 * // Discovery
 * auto names = registry.getReductionNames();
 * auto for_events = registry.getReductionsForInputType<EventWithId>();
 *
 * // Type-safe execution
 * auto latency = registry.execute<EventWithId, float, FirstLatencyParams>(
 *     "FirstPositiveLatency", events, params);
 * ```
 *
 * @see RangeReductionTypes.hpp for concepts and metadata
 * @see ElementRegistry.hpp for parallel element transform registry
 */

#include "RangeReductionTypes.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <any>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace WhiskerToolbox::Transforms::V2 {

// ============================================================================
// Type Key for Reduction Lookup
// ============================================================================

/**
 * @brief Key for looking up reductions by name and input type
 */
struct ReductionKey {
    std::type_index input_type;
    std::string name;

    bool operator==(ReductionKey const & other) const {
        return input_type == other.input_type && name == other.name;
    }
};

/**
 * @brief Hash function for ReductionKey
 */
struct ReductionKeyHash {
    std::size_t operator()(ReductionKey const & key) const {
        std::size_t h1 = std::hash<std::type_index>{}(key.input_type);
        std::size_t h2 = std::hash<std::string>{}(key.name);
        return h1 ^ (h2 << 1);
    }
};

/**
 * @brief Triple key for typed lookup (input, output, params)
 */
struct ReductionTypeTriple {
    std::type_index input_type;
    std::type_index output_type;
    std::type_index params_type;

    bool operator==(ReductionTypeTriple const & other) const {
        return input_type == other.input_type &&
               output_type == other.output_type &&
               params_type == other.params_type;
    }
};

struct ReductionTypeTripleHash {
    std::size_t operator()(ReductionTypeTriple const & key) const {
        std::size_t h1 = std::hash<std::type_index>{}(key.input_type);
        std::size_t h2 = std::hash<std::type_index>{}(key.output_type);
        std::size_t h3 = std::hash<std::type_index>{}(key.params_type);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

// ============================================================================
// Type-Erased Reduction Interface
// ============================================================================

/**
 * @brief Base interface for type-erased range reductions
 *
 * Allows storage of heterogeneous reductions in a single map.
 */
class IRangeReduction {
public:
    virtual ~IRangeReduction() = default;

    /**
     * @brief Execute reduction with type-erased input/output
     *
     * @param input_range std::any containing std::span<Element const>
     * @param params std::any containing the parameters
     * @return std::any containing the scalar result
     * @throws std::bad_any_cast if types don't match
     */
    virtual std::any executeErased(std::any const & input_range, std::any const & params) const = 0;
};

/**
 * @brief Typed implementation of range reduction
 *
 * @tparam Element Input element type
 * @tparam Scalar Output scalar type
 * @tparam Params Parameter type
 */
template<typename Element, typename Scalar, typename Params>
class TypedRangeReduction : public IRangeReduction {
public:
    using FuncType = std::function<Scalar(std::span<Element const>, Params const &)>;

    explicit TypedRangeReduction(FuncType func)
        : func_(std::move(func)) {}

    /**
     * @brief Type-safe execution
     */
    Scalar execute(std::span<Element const> input, Params const & params) const {
        return func_(input, params);
    }

    /**
     * @brief Type-erased execution for pipeline use
     */
    std::any executeErased(std::any const & input_any, std::any const & params_any) const override {
        auto const & input = std::any_cast<std::span<Element const>>(input_any);
        auto const & params = std::any_cast<Params const &>(params_any);
        return func_(input, params);
    }

private:
    FuncType func_;
};

// ============================================================================
// Type-Erased Parameter Executor (for pipeline use)
// ============================================================================

/**
 * @brief Interface for executing reductions with captured parameters
 *
 * This allows the pipeline to execute reductions without knowing the
 * parameter type - parameters are captured at construction time.
 */
class IRangeReductionParamExecutor {
public:
    virtual ~IRangeReductionParamExecutor() = default;

    /**
     * @brief Execute reduction with captured parameters and function
     *
     * @param input_range std::any containing std::span<Element const>
     * @return std::any containing the scalar result
     */
    virtual std::any execute(std::any const & input_range) const = 0;

    /**
     * @brief Get the output type index for this executor
     */
    virtual std::type_index outputType() const = 0;
};

/**
 * @brief Typed parameter executor with captured parameters and reduction function
 *
 * @tparam Element Input element type
 * @tparam Scalar Output scalar type
 * @tparam Params Parameter type
 */
template<typename Element, typename Scalar, typename Params>
class TypedRangeReductionParamExecutor : public IRangeReductionParamExecutor {
public:
    using ReductionPtr = std::shared_ptr<TypedRangeReduction<Element, Scalar, Params>>;

    TypedRangeReductionParamExecutor(ReductionPtr reduction, Params params)
        : reduction_(std::move(reduction)), params_(std::move(params)) {}

    std::any execute(std::any const & input_any) const override {
        auto const & input = std::any_cast<std::span<Element const>>(input_any);
        return reduction_->execute(input, params_);
    }

    std::type_index outputType() const override {
        return std::type_index(typeid(Scalar));
    }

private:
    ReductionPtr reduction_;
    Params params_;
};

// ============================================================================
// Range Reduction Registry
// ============================================================================

/**
 * @brief Registry for range reduction operations
 *
 * Provides registration, discovery, and execution of range reductions.
 * Follows the singleton pattern like ElementRegistry.
 */
class RangeReductionRegistry {
public:
    RangeReductionRegistry() = default;

    // Non-copyable (singleton usage)
    RangeReductionRegistry(RangeReductionRegistry const &) = delete;
    RangeReductionRegistry & operator=(RangeReductionRegistry const &) = delete;
    RangeReductionRegistry(RangeReductionRegistry &&) = default;
    RangeReductionRegistry & operator=(RangeReductionRegistry &&) = default;

    /**
     * @brief Get global singleton instance
     */
    static RangeReductionRegistry & instance() {
        static RangeReductionRegistry registry;
        return registry;
    }

    // ========================================================================
    // Registration
    // ========================================================================

    /**
     * @brief Register a range reduction
     *
     * @tparam Element Input element type (e.g., EventWithId)
     * @tparam Scalar Output scalar type (e.g., float)
     * @tparam Params Parameter type (use NoReductionParams for stateless)
     *
     * @param name Unique reduction name
     * @param func Reduction function: (span<Element const>, Params const&) -> Scalar
     * @param metadata Optional metadata for UI/discovery
     */
    template<typename Element, typename Scalar, typename Params>
    void registerReduction(
            std::string const & name,
            std::function<Scalar(std::span<Element const>, Params const &)> func,
            RangeReductionMetadata metadata = {}) {

        // Create typed reduction
        auto reduction = std::make_shared<TypedRangeReduction<Element, Scalar, Params>>(std::move(func));

        // Complete metadata
        metadata.name = name;
        metadata.input_type = std::type_index(typeid(Element));
        metadata.output_type = std::type_index(typeid(Scalar));
        metadata.params_type = std::type_index(typeid(Params));
        metadata.input_type_name = typeid(Element).name();
        metadata.output_type_name = typeid(Scalar).name();
        metadata.params_type_name = typeid(Params).name();

        // Store reduction
        ReductionKey key{std::type_index(typeid(Element)), name};
        reductions_[key] = reduction;
        metadata_[name] = metadata;

        // Update lookup maps
        input_type_to_names_[std::type_index(typeid(Element))].push_back(name);
        output_type_to_names_[std::type_index(typeid(Scalar))].push_back(name);

        // Register parameter handling
        registerParamHandling<Element, Scalar, Params>(name);
    }

    /**
     * @brief Register a stateless reduction (no parameters)
     */
    template<typename Element, typename Scalar>
    void registerStatelessReduction(
            std::string const & name,
            std::function<Scalar(std::span<Element const>)> func,
            RangeReductionMetadata metadata = {}) {

        // Wrap as parameterized with NoReductionParams
        auto wrapped = [f = std::move(func)](std::span<Element const> input,
                                              NoReductionParams const &) -> Scalar {
            return f(input);
        };

        registerReduction<Element, Scalar, NoReductionParams>(name, std::move(wrapped), metadata);
    }

    // ========================================================================
    // Typed Lookup
    // ========================================================================

    /**
     * @brief Get a typed reduction by name
     *
     * @return Shared pointer to reduction, or nullptr if not found
     */
    template<typename Element, typename Scalar, typename Params>
    std::shared_ptr<TypedRangeReduction<Element, Scalar, Params>>
    getReduction(std::string const & name) const {
        ReductionKey key{std::type_index(typeid(Element)), name};
        auto it = reductions_.find(key);
        if (it == reductions_.end()) {
            return nullptr;
        }
        return std::dynamic_pointer_cast<TypedRangeReduction<Element, Scalar, Params>>(it->second);
    }

    // ========================================================================
    // Type-Safe Execution
    // ========================================================================

    /**
     * @brief Execute a reduction with full type safety
     *
     * @throws std::runtime_error if reduction not found or type mismatch
     */
    template<typename Element, typename Scalar, typename Params>
    Scalar execute(std::string const & name,
                   std::span<Element const> input,
                   Params const & params) const {

        auto reduction = getReduction<Element, Scalar, Params>(name);
        if (!reduction) {
            throw std::runtime_error("Range reduction not found: " + name);
        }
        return reduction->execute(input, params);
    }

    // ========================================================================
    // Discovery API
    // ========================================================================

    /**
     * @brief Get all registered reduction names
     */
    std::vector<std::string> getReductionNames() const {
        std::vector<std::string> names;
        names.reserve(metadata_.size());
        for (auto const & [name, _] : metadata_) {
            names.push_back(name);
        }
        return names;
    }

    /**
     * @brief Get reductions for a specific input element type
     */
    template<typename Element>
    std::vector<std::string> getReductionsForInputType() const {
        auto it = input_type_to_names_.find(std::type_index(typeid(Element)));
        if (it == input_type_to_names_.end()) {
            return {};
        }
        return it->second;
    }

    /**
     * @brief Get reductions for a specific input type (type-erased)
     */
    std::vector<std::string> getReductionsForInputType(std::type_index input_type) const {
        auto it = input_type_to_names_.find(input_type);
        if (it == input_type_to_names_.end()) {
            return {};
        }
        return it->second;
    }

    /**
     * @brief Get reductions that produce a specific output type
     */
    template<typename Scalar>
    std::vector<std::string> getReductionsForOutputType() const {
        auto it = output_type_to_names_.find(std::type_index(typeid(Scalar)));
        if (it == output_type_to_names_.end()) {
            return {};
        }
        return it->second;
    }

    /**
     * @brief Get metadata for a reduction
     *
     * @return Pointer to metadata, or nullptr if not found
     */
    RangeReductionMetadata const * getMetadata(std::string const & name) const {
        auto it = metadata_.find(name);
        if (it == metadata_.end()) {
            return nullptr;
        }
        return &it->second;
    }

    /**
     * @brief Get all metadata entries
     */
    std::unordered_map<std::string, RangeReductionMetadata> const & getAllMetadata() const {
        return metadata_;
    }

    /**
     * @brief Check if a reduction is registered
     */
    bool hasReduction(std::string const & name) const {
        return metadata_.find(name) != metadata_.end();
    }

    /**
     * @brief Check if a reduction exists for the given input type
     */
    template<typename Element>
    bool hasReductionForType(std::string const & name) const {
        ReductionKey key{std::type_index(typeid(Element)), name};
        return reductions_.find(key) != reductions_.end();
    }

    // ========================================================================
    // Parameter Executor Factory (for pipeline use)
    // ========================================================================

    /**
     * @brief Create a parameter executor with captured parameters
     *
     * This is used by the pipeline to execute reductions without
     * knowing the parameter type at runtime.
     *
     * @param name Reduction name to look up
     * @param params Parameters to capture
     * @return Unique pointer to executor, or nullptr if reduction not found
     */
    template<typename Element, typename Scalar, typename Params>
    std::unique_ptr<IRangeReductionParamExecutor>
    createParamExecutor(std::string const & name, Params params) const {
        auto reduction = getReduction<Element, Scalar, Params>(name);
        if (!reduction) {
            return nullptr;
        }
        return std::make_unique<TypedRangeReductionParamExecutor<Element, Scalar, Params>>(
                std::move(reduction), std::move(params));
    }

    /**
     * @brief Deserialize parameters from JSON and create executor
     *
     * @param name Reduction name
     * @param json_params JSON string containing parameters
     * @return Unique pointer to executor, or nullptr if not found
     * @throws rfl::Error if JSON parsing fails
     */
    std::unique_ptr<IRangeReductionParamExecutor>
    createParamExecutorFromJson(std::string const & name,
                                std::string const & json_params) const {
        auto it = param_executor_factories_.find(name);
        if (it == param_executor_factories_.end()) {
            return nullptr;
        }
        return it->second(json_params);
    }

    /**
     * @brief Deserialize parameters from JSON string
     *
     * Looks up the reduction's parameter type and uses the registered deserializer.
     *
     * @param reduction_name Name of the reduction
     * @param json_str JSON string containing parameters
     * @return std::any containing typed parameters, or empty std::any on failure
     */
    std::any deserializeParameters(
            std::string const & reduction_name,
            std::string const & json_str) const {
        // Get metadata to find parameter type
        auto const * meta = getMetadata(reduction_name);
        if (!meta) {
            return std::any{};  // Reduction not found
        }

        // Look up deserializer for this parameter type
        auto it = param_deserializers_.find(meta->params_type);
        if (it == param_deserializers_.end()) {
            return std::any{};  // No deserializer registered
        }

        // Deserialize
        try {
            return it->second(json_str);
        } catch (...) {
            return std::any{};  // Deserialization failed
        }
    }

    // ========================================================================
    // Type-Erased Execution (for pipeline use)
    // ========================================================================

    /**
     * @brief Execute reduction with type-erased input and parameters
     *
     * @param name Reduction name
     * @param input_type Type of input elements
     * @param input_range std::any containing std::span<Element const>
     * @param params std::any containing parameters
     * @return std::any containing scalar result
     * @throws std::runtime_error if reduction not found
     */
    std::any executeErased(std::string const & name,
                           std::type_index input_type,
                           std::any const & input_range,
                           std::any const & params) const {
        ReductionKey key{input_type, name};
        auto it = reductions_.find(key);
        if (it == reductions_.end()) {
            throw std::runtime_error("Range reduction not found: " + name);
        }
        return it->second->executeErased(input_range, params);
    }

private:
    // ========================================================================
    // Parameter Handling Registration
    // ========================================================================

    /**
     * @brief Register parameter deserializer and executor factory
     */
    template<typename Element, typename Scalar, typename Params>
    void registerParamHandling(std::string const & name) {
        // Capture the reduction pointer for the factory
        ReductionKey key{std::type_index(typeid(Element)), name};
        auto reduction = std::dynamic_pointer_cast<TypedRangeReduction<Element, Scalar, Params>>(
                reductions_[key]);

        // Register factory that creates executor from JSON
        // Capture the reduction by value (shared_ptr copy)
        param_executor_factories_[name] = [reduction](std::string const & json) {
            auto params = rfl::json::read<Params>(json);
            if (!params) {
                throw std::runtime_error("Failed to parse reduction parameters: " +
                                         std::string(params.error()->what()));
            }
            return std::make_unique<TypedRangeReductionParamExecutor<Element, Scalar, Params>>(
                    reduction, std::move(*params));
        };

        // Register parameter deserializer (by params_type for lookup via metadata)
        auto params_type_index = std::type_index(typeid(Params));
        if (param_deserializers_.find(params_type_index) == param_deserializers_.end()) {
            param_deserializers_[params_type_index] = [](std::string const & json) -> std::any {
                auto result = rfl::json::read<Params>(json);
                if (!result) {
                    throw std::runtime_error("Failed to parse parameters: " +
                                             std::string(result.error()->what()));
                }
                return std::any{std::move(*result)};
            };
        }

        // Store type triple for discovery
        ReductionTypeTriple triple{
                std::type_index(typeid(Element)),
                std::type_index(typeid(Scalar)),
                std::type_index(typeid(Params))};
        type_to_name_[triple] = name;
    }

    // Storage
    std::unordered_map<ReductionKey, std::shared_ptr<IRangeReduction>, ReductionKeyHash> reductions_;
    std::unordered_map<std::string, RangeReductionMetadata> metadata_;

    // Lookup maps
    std::unordered_map<std::type_index, std::vector<std::string>> input_type_to_names_;
    std::unordered_map<std::type_index, std::vector<std::string>> output_type_to_names_;

    // Parameter executor factories (name -> factory that creates executor from JSON)
    std::unordered_map<std::string,
                       std::function<std::unique_ptr<IRangeReductionParamExecutor>(std::string const &)>>
            param_executor_factories_;

    // Parameter deserializers (params_type -> deserializer function)
    std::unordered_map<std::type_index, std::function<std::any(std::string const &)>>
            param_deserializers_;

    // Type triple to name mapping
    std::unordered_map<ReductionTypeTriple, std::string, ReductionTypeTripleHash> type_to_name_;
};

}// namespace WhiskerToolbox::Transforms::V2

#endif// WHISKERTOOLBOX_V2_RANGE_REDUCTION_REGISTRY_HPP
