#ifndef PARAM_EXECUTOR_HPP
#define PARAM_EXECUTOR_HPP

#include "transforms/v2/extension/TransformTypes.hpp"

#include <any>
#include <string>
#include <typeindex>
#include <variant>

namespace WhiskerToolbox::Transforms::V2 {

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

    bool operator==(TypeTriple const & other) const {
        return input_type == other.input_type &&
               output_type == other.output_type &&
               params_type == other.params_type;
    }
};

/**
 * @brief Hash function for TypeTriple
 */
struct TypeTripleHash {
    std::size_t operator()(TypeTriple const & key) const {
        std::size_t h1 = std::hash<std::type_index>{}(key.input_type);
        std::size_t h2 = std::hash<std::type_index>{}(key.output_type);
        std::size_t h3 = std::hash<std::type_index>{}(key.params_type);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

// Helper to check if type T is in Variant
template<typename T, typename Variant>
struct is_in_variant;

template<typename T, typename... Ts>
struct is_in_variant<T, std::variant<Ts...>> : std::disjunction<std::is_same<T, Ts>...> {};

template<typename T, typename Variant>
inline constexpr bool is_in_variant_v = is_in_variant<T, Variant>::value;

// is_tuple is defined in ElementTransform.hpp

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
    virtual ElementVariant execute(std::string const & name, ElementVariant const & input) const = 0;

    // New method to support arbitrary input types (e.g. tuples) for the "Head" of the pipeline
    virtual ElementVariant executeAny(std::string const & name, std::any const & input) const = 0;
};

/**
 * @brief Interface for time-grouped parameter executors with captured state
 */
class ITimeGroupedParamExecutor {
public:
    virtual ~ITimeGroupedParamExecutor() = default;
    virtual BatchVariant execute(std::string const & name, BatchVariant const & input_span) const = 0;
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

    ElementVariant execute(std::string const & name, ElementVariant const & input_any) const override;

    ElementVariant executeAny(std::string const & name, std::any const & input_any) const override;

private:
    Params params_;// Parameters captured at construction
};

/**
 * @brief Concrete time-grouped executor with full type information and captured parameters
 */
template<typename In, typename Out, typename Params>
class TypedTimeGroupedParamExecutor : public ITimeGroupedParamExecutor {
public:
    explicit TypedTimeGroupedParamExecutor(Params params);

    BatchVariant execute(std::string const & name, BatchVariant const & input_span_any) const override;

private:
    Params params_;
};

} // namespace WhiskerToolbox::Transforms::V2


#endif // PARAM_EXECUTOR_HPP