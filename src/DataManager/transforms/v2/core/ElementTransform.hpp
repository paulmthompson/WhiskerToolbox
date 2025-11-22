#ifndef WHISKERTOOLBOX_V2_ELEMENT_TRANSFORM_HPP
#define WHISKERTOOLBOX_V2_ELEMENT_TRANSFORM_HPP

#include <concepts>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <tuple>
#include <type_traits>

namespace WhiskerToolbox::Transforms::V2 {

/**
 * @brief Context for transform execution
 * 
 * Provides additional information and services during transform execution:
 * - Progress reporting
 * - Cancellation checking
 * - Logging
 * - Provenance tracking
 */
struct ComputeContext {
    using ProgressCallback = std::function<void(int progress)>;
    using CancellationCheck = std::function<bool()>;
    using Logger = std::function<void(std::string const& message)>;
    
    ProgressCallback progress;
    CancellationCheck is_cancelled;
    Logger log;
    
    // Provenance tracking (optional)
    void* provenance_tracker = nullptr;
    
    // Helper methods
    void reportProgress(int p) const {
        if (progress) { progress(p); }
    }
    
    bool shouldCancel() const {
        return is_cancelled && is_cancelled();
    }
    
    void logMessage(std::string const& msg) const {
        if (log) { log(msg); }
    }
};

// ============================================================================
// Concepts for Element Transforms
// ============================================================================

/**
 * @brief Concept for stateless element-level transform
 * 
 * A pure function: In → Out
 * No parameters, no state, just data transformation.
 */
template<typename F, typename In, typename Out>
concept StatelessElementTransform = requires(F f, In const& in) {
    { f(in) } -> std::convertible_to<Out>;
};

/**
 * @brief Concept for parameterized element-level transform
 * 
 * Function with parameters: (In, Params) → Out
 * Most transforms fall into this category.
 */
template<typename F, typename In, typename Out, typename Params>
concept ParameterizedElementTransform = requires(F f, In const& in, Params const& p) {
    { f(in, p) } -> std::convertible_to<Out>;
};

/**
 * @brief Concept for context-aware element transform
 * 
 * Function with parameters and context: (In, Params, Context) → Out
 * For transforms that need progress reporting, cancellation, etc.
 */
template<typename F, typename In, typename Out, typename Params>
concept ContextAwareElementTransform = requires(F f, In const& in, Params const& p, ComputeContext const& ctx) {
    { f(in, p, ctx) } -> std::convertible_to<Out>;
};

/**
 * @brief Concept for any valid element transform
 */
template<typename F, typename In, typename Out, typename Params = void>
concept ElementTransform = 
    StatelessElementTransform<F, In, Out> ||
    ParameterizedElementTransform<F, In, Out, Params> ||
    ContextAwareElementTransform<F, In, Out, Params>;

// ============================================================================
// Multi-Input Transform Concepts
// ============================================================================

/**
 * @brief Concept for binary element transform
 * 
 * Takes two inputs: (In1, In2, Params) → Out
 */
template<typename F, typename In1, typename In2, typename Out, typename Params>
concept BinaryElementTransform = requires(F f, In1 const& in1, In2 const& in2, Params const& p) {
    { f(in1, in2, p) } -> std::convertible_to<Out>;
};

// Note: Variadic element transforms removed - use tuples for multi-input
// e.g., std::tuple<In1, In2, In3> for 3 inputs

// ============================================================================
// Time-Grouped Transform Concepts (M→N per time point)
// ============================================================================

/**
 * @brief Concept for time-grouped transform
 * 
 * Operates on all elements at a single time point:
 * Range<In> → Range<Out>
 * 
 * This enables M→N transformations per time:
 * - Sum reduction: many floats → one float (M→1)
 * - Filter: many floats → variable floats (M→N, N≤M)
 * - Decompose: one mask → many masks (1→M)
 * 
 * The transform receives all elements at a time and produces all outputs for that time.
 */
template<typename F, typename In, typename Out, typename Params>
concept TimeGroupedTransform = requires(
    F f,
    std::span<In const> inputs,
    Params const& params) {
    { f(inputs, params) } -> std::convertible_to<std::vector<Out>>;
};

/**
 * @brief Concept for context-aware time-grouped transform
 */
template<typename F, typename In, typename Out, typename Params>
concept ContextAwareTimeGroupedTransform = requires(
    F f,
    std::span<In const> inputs,
    Params const& params,
    ComputeContext const& ctx) {
    { f(inputs, params, ctx) } -> std::convertible_to<std::vector<Out>>;
};

// ============================================================================
// Type Traits for Transform Inspection
// ============================================================================

/**
 * @brief Check if type is a tuple
 */
template<typename T>
struct is_tuple : std::false_type {};

template<typename... Ts>
struct is_tuple<std::tuple<Ts...>> : std::true_type {};

template<typename T>
inline constexpr bool is_tuple_v = is_tuple<T>::value;

/**
 * @brief Get number of inputs (1 for single type, N for tuple)
 */
template<typename In>
struct input_arity {
    static constexpr size_t value = 1;
};

template<typename... Ins>
struct input_arity<std::tuple<Ins...>> {
    static constexpr size_t value = sizeof...(Ins);
};

template<typename In>
inline constexpr size_t input_arity_v = input_arity<In>::value;

/**
 * @brief Extract individual types from tuple or return single type
 */
template<typename T>
struct input_types {
    using type = std::tuple<T>;
};

template<typename... Ts>
struct input_types<std::tuple<Ts...>> {
    using type = std::tuple<Ts...>;
};

template<typename T>
using input_types_t = typename input_types<T>::type;

// ============================================================================
// Typed Transform Wrapper
// ============================================================================

/**
 * @brief Compile-time typed transform function wrapper
 * 
 * Wraps a transform function with full type information.
 * Supports:
 * - Single input or tuple of inputs
 * - With or without parameters
 * - With or without context
 * 
 * @tparam In Input type (single or std::tuple<...>)
 * @tparam Out Output type
 * @tparam Params Parameter type (void if none)
 */
template<typename In, typename Out, typename Params = void>
class TypedTransform {
public:
    using InputType = In;
    using OutputType = Out;
    using ParamsType = Params;
    
    // Function signature with context
    using FunctionType = std::function<Out(In const&, Params const&, ComputeContext const&)>;
    
    /**
     * @brief Construct from context-aware function
     */
    explicit TypedTransform(FunctionType func)
        : func_(std::move(func)) {}
    
    /**
     * @brief Construct from parameterized function (no context)
     */
    template<typename F>
    requires ParameterizedElementTransform<F, In, Out, Params>
    explicit TypedTransform(F func)
        : func_([f = std::move(func)](In const& in, Params const& p, ComputeContext const&) {
            return f(in, p);
        }) {}
    
    /**
     * @brief Construct from stateless function (no params, no context)
     */
    template<typename F>
    requires StatelessElementTransform<F, In, Out> && std::is_void_v<Params>
    explicit TypedTransform(F func)
        : func_([f = std::move(func)](In const& in, Params const&, ComputeContext const&) {
            return f(in);
        }) {}
    
    /**
     * @brief Execute transform with full context
     */
    Out execute(In const& input, Params const& params, ComputeContext const& ctx) const {
        return func_(input, params, ctx);
    }
    
    /**
     * @brief Execute transform without explicit context
     */
    Out execute(In const& input, Params const& params) const {
        ComputeContext default_ctx;
        return func_(input, params, default_ctx);
    }
    
    /**
     * @brief Execute stateless transform (no params)
     */
    Out execute(In const& input) const requires std::is_void_v<Params> {
        Params dummy{};
        ComputeContext default_ctx;
        return func_(input, dummy, default_ctx);
    }
    
    /**
     * @brief Check if this is a multi-input transform
     */
    static constexpr bool isMultiInput() {
        return is_tuple_v<In>;
    }
    
    /**
     * @brief Get number of inputs
     */
    static constexpr size_t getInputArity() {
        return input_arity_v<In>;
    }

private:
    FunctionType func_;
};

// ============================================================================
// Specialization for Void Parameters
// ============================================================================

/**
 * @brief Specialization for transforms with no parameters
 */
template<typename In, typename Out>
class TypedTransform<In, Out, void> {
public:
    using InputType = In;
    using OutputType = Out;
    using ParamsType = void;
    
    using FunctionType = std::function<Out(In const&, ComputeContext const&)>;
    
    explicit TypedTransform(FunctionType func)
        : func_(std::move(func)) {}
    
    template<typename F>
    requires StatelessElementTransform<F, In, Out>
    explicit TypedTransform(F func)
        : func_([f = std::move(func)](In const& in, ComputeContext const&) {
            return f(in);
        }) {}
    
    Out execute(In const& input, ComputeContext const& ctx) const {
        return func_(input, ctx);
    }
    
    Out execute(In const& input) const {
        ComputeContext default_ctx;
        return func_(input, default_ctx);
    }
    
    static constexpr bool isMultiInput() {
        return is_tuple_v<In>;
    }
    
    static constexpr size_t getInputArity() {
        return input_arity_v<In>;
    }

private:
    FunctionType func_;
};

// ============================================================================
// Typed Time-Grouped Transform Wrapper (M→N per time point)
// ============================================================================

/**
 * @brief Type-safe wrapper for time-grouped transforms
 * 
 * Wraps transforms that operate on all elements at a single time point.
 * Signature: (span<In const>, Params const&) → vector<Out>
 * 
 * Example: Sum reduction that takes many floats and produces one
 * 
 * @tparam In Input element type
 * @tparam Out Output element type
 * @tparam Params Parameter type (use void for stateless)
 */
template<typename In, typename Out, typename Params = void>
class TypedTimeGroupedTransform {
public:
    using InputType = In;
    using OutputType = Out;
    using ParamsType = Params;
    using FunctionType = std::function<std::vector<Out>(std::span<In const>, Params const&)>;
    
    explicit TypedTimeGroupedTransform(FunctionType func)
        : func_(std::move(func)) {}
    
    // Construct from any compatible callable
    template<typename F>
    requires std::invocable<F, std::span<In const>, Params const&>
    explicit TypedTimeGroupedTransform(F func)
        : func_(std::move(func)) {}
    
    std::vector<Out> execute(std::span<In const> inputs, Params const& params) const {
        return func_(inputs, params);
    }

private:
    FunctionType func_;
};

/**
 * @brief Specialization for stateless time-grouped transforms (no parameters)
 */
template<typename In, typename Out>
class TypedTimeGroupedTransform<In, Out, void> {
public:
    using InputType = In;
    using OutputType = Out;
    using ParamsType = void;
    using FunctionType = std::function<std::vector<Out>(std::span<In const>)>;
    
    explicit TypedTimeGroupedTransform(FunctionType func)
        : func_(std::move(func)) {}
    
    template<typename F>
    requires std::invocable<F, std::span<In const>>
    explicit TypedTimeGroupedTransform(F func)
        : func_(std::move(func)) {}
    
    std::vector<Out> execute(std::span<In const> inputs) const {
        return func_(inputs);
    }

private:
    FunctionType func_;
};

// ============================================================================
// Transform Composition Helpers
// ============================================================================

/**
 * @brief Compose two transforms: f ∘ g
 * 
 * Creates a new transform that applies g then f.
 * Types must be compatible: In → Mid → Out
 */
template<typename In, typename Mid, typename Out, typename Params1, typename Params2>
auto compose(TypedTransform<In, Mid, Params1> const& first,
            TypedTransform<Mid, Out, Params2> const& second) {
    
    using CombinedParams = std::pair<Params1, Params2>;
    
    return TypedTransform<In, Out, CombinedParams>(
        [first, second](In const& input, CombinedParams const& params, ComputeContext const& ctx) {
            auto mid_result = first.execute(input, params.first, ctx);
            return second.execute(mid_result, params.second, ctx);
        }
    );
}

/**
 * @brief Chain multiple transforms
 * 
 * Applies transforms in sequence, threading output of each into the next.
 */
template<typename... Transforms>
class TransformChain {
public:
    explicit TransformChain(Transforms... transforms)
        : transforms_(std::make_tuple(std::move(transforms)...)) {}
    
    template<typename In, typename Params>
    auto execute(In const& input, Params const& params, ComputeContext const& ctx) const {
        return executeImpl(input, params, ctx, std::index_sequence_for<Transforms...>{});
    }

private:
    template<typename In, typename Params, size_t... Is>
    auto executeImpl(In const& input, Params const& params, ComputeContext const& ctx, 
                    std::index_sequence<Is...>) const {
        auto current = input;
        ((current = std::get<Is>(transforms_).execute(current, std::get<Is>(params), ctx)), ...);
        return current;
    }
    
    std::tuple<Transforms...> transforms_;
};

} // namespace WhiskerToolbox::Transforms::V2

#endif // WHISKERTOOLBOX_V2_ELEMENT_TRANSFORM_HPP
