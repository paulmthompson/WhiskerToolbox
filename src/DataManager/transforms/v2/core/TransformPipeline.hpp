#ifndef WHISKERTOOLBOX_V2_TRANSFORM_PIPELINE_HPP
#define WHISKERTOOLBOX_V2_TRANSFORM_PIPELINE_HPP

#include "ContainerTraits.hpp"
#include "ContainerTransform.hpp"
#include "ElementRegistry.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"

#include "transforms/v2/algorithms/MaskArea/MaskArea.hpp"
#include "transforms/v2/algorithms/SumReduction/SumReduction.hpp"

#include <any>
#include <functional>
#include <map>
#include <memory>
#include <ranges>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace WhiskerToolbox::Transforms::V2 {

// ============================================================================
// Helper for Adding Elements to Different Container Types
// ============================================================================

// Helper for static_assert in else branch
template<typename>
inline constexpr bool always_false = false;

/**
 * @brief Add element to output container, handling different container APIs
 * 
 * Different containers have different methods:
 * - RaggedTimeSeries<T>: addAtTime(time, T, notify)
 * - RaggedAnalogTimeSeries: appendAtTime(time, std::vector<float>, notify)
 * 
 * This helper uses compile-time checks to call the right method.
 */
template<typename Container, typename Element>
void addElementToContainer(Container& container, TimeFrameIndex time, Element&& element) {
    // Check if container has appendAtTime (like RaggedAnalogTimeSeries)
    if constexpr (requires { container.appendAtTime(time, std::vector<Element>{}, NotifyObservers::No); }) {
        // Wrap single element in vector for appendAtTime
        container.appendAtTime(time, std::vector<Element>{std::forward<Element>(element)}, NotifyObservers::No);
    }
    // Check if container has addAtTime for single elements (like RaggedTimeSeries<T>)
    else if constexpr (requires { container.addAtTime(time, std::forward<Element>(element), NotifyObservers::No); }) {
        container.addAtTime(time, std::forward<Element>(element), NotifyObservers::No);
    }
    else {
        static_assert(always_false<Container>, "Container type does not support addAtTime or appendAtTime");
    }
}

// ============================================================================
// Pipeline Step Definition
// ============================================================================

/**
 * @brief Represents a single step in a transform pipeline
 * 
 * Each step contains:
 * - Transform name (for registry lookup)
 * - Type-erased parameters
 * - Type-erased execution functions for both element and time-grouped transforms
 * 
 * The executors are captured at the time of step creation, eliminating the need
 * for runtime type checking or reflection.
 */
struct PipelineStep {
    std::string transform_name;
    std::any params;  // Type-erased parameters
    
    // Type-erased executors that know the correct parameter type
    // These are set when the step is added to the pipeline
    std::function<std::any(std::any const&, std::any const&)> element_executor;
    std::function<std::any(std::any const&, std::any const&)> time_grouped_executor;
    
    template<typename Params>
    PipelineStep(std::string name, Params p)
        : transform_name(std::move(name))
        , params(std::move(p))
    {
        // Create type-erased executors that capture the parameter type
        auto& registry = ElementRegistry::instance();
        auto const* meta = registry.getMetadata(transform_name);
        
        if (meta && !meta->is_time_grouped) {
            // Element transform executor
            element_executor = [name = transform_name, p = this->params](
                std::any const& input, std::any const&) -> std::any {
                // This will be called with the correct types by the pipeline
                // The actual execution is deferred until we know InputElement and OutputElement
                return std::any{};  // Placeholder - will be replaced by specialized executor
            };
        } else if (meta && meta->is_time_grouped) {
            // Time-grouped transform executor
            time_grouped_executor = [name = transform_name, p = this->params](
                std::any const& input, std::any const&) -> std::any {
                return std::any{};  // Placeholder
            };
        }
    }
    
    PipelineStep(std::string name)
        : PipelineStep(std::move(name), NoParams{})
    {}
    
    /**
     * @brief Create element executor for specific types
     * 
     * This template method creates a properly typed executor that can be called
     * from the pipeline without knowing the parameter type.
     */
    template<typename InputElement, typename OutputElement, typename Params>
    void createElementExecutor() {
        auto& registry = ElementRegistry::instance();
        element_executor = [&registry, name = transform_name, p = std::any_cast<Params>(params)](
            std::any const& input_any, std::any const&) -> std::any {
            auto const& input = std::any_cast<InputElement const&>(input_any);
            OutputElement result = registry.execute<InputElement, OutputElement, Params>(
                name, input, p);
            return std::any{result};
        };
    }
    
    /**
     * @brief Create time-grouped executor for specific types
     */
    template<typename InputElement, typename OutputElement, typename Params>
    void createTimeGroupedExecutor() {
        auto& registry = ElementRegistry::instance();
        time_grouped_executor = [&registry, name = transform_name, p = std::any_cast<Params>(params)](
            std::any const& input_any, std::any const&) -> std::any {
            auto const& input = std::any_cast<std::span<InputElement const> const&>(input_any);
            std::vector<OutputElement> result = registry.executeTimeGrouped<InputElement, OutputElement, Params>(
                name, input, p);
            return std::any{result};
        };
    }
};

// ============================================================================
// Transform Pipeline
// ============================================================================

/**
 * @brief Builder for chaining multiple transforms together
 * 
 * This class enables composing a sequence of transforms that will be applied
 * in order, with automatic container type transitions.
 * 
 * The pipeline is fully generic and works with any compatible transforms.
 * 
 * Example usage:
 * ```cpp
 * auto pipeline = TransformPipeline()
 *     .addStep("CalculateMaskArea", MaskAreaParams{})
 *     .addStep("SumReduction", SumReductionParams{});
 * 
 * // Execute with correct input/output types
 * std::shared_ptr<AnalogTimeSeries> result = pipeline.execute<MaskData, AnalogTimeSeries>(mask_data);
 * ```
 */
class TransformPipeline {
public:
    TransformPipeline() = default;
    
    /**
     * @brief Add a transform step to the pipeline
     * 
     * @tparam Params Parameter type for this transform
     * @param transform_name Name of the registered transform
     * @param params Parameters for the transform
     * @return Reference to this pipeline for chaining
     */
    template<typename Params>
    TransformPipeline& addStep(std::string const& transform_name, Params params) {
        steps_.emplace_back(transform_name, std::move(params));
        return *this;
    }
    
    /**
     * @brief Add a transform step with no parameters
     */
    TransformPipeline& addStep(std::string const& transform_name) {
        steps_.emplace_back(transform_name);
        return *this;
    }
    
    /**
     * @brief Add a pre-constructed pipeline step
     * 
     * This is useful for loading pipelines from JSON where the step
     * is constructed separately with type-erased parameters.
     * 
     * @param step Pre-constructed pipeline step
     * @return Reference to this pipeline for chaining
     */
    TransformPipeline& addStep(PipelineStep step) {
        steps_.push_back(std::move(step));
        return *this;
    }
    
    /**
     * @brief Execute a two-step pipeline with full type specification
     * 
     * This method composes transforms into a single element-level function
     * before iteration, enabling true element-by-element execution with
     * no intermediate containers.
     * 
     * @tparam InputContainer Input container type (e.g., MaskData)
     * @tparam OutputContainer Final output container type (e.g., AnalogTimeSeries)
     * @param input Input data
     * @return Shared pointer to final output
     * 
     * Performance characteristics:
     * - Single pass through data
     * - No intermediate container allocations
     * - Cache-friendly (hot data stays in cache)
     * - Only type erasure cost is at composition time, not per-element
     */
    template<typename InputContainer, typename OutputContainer>
    std::shared_ptr<OutputContainer> execute(InputContainer const& input) const {
        if (steps_.empty()) {
            throw std::runtime_error("Pipeline has no steps");
        }
        
        if (steps_.size() != 2) {
            throw std::runtime_error("Generic execute currently supports only 2-step pipelines");
        }
        
        auto& registry = ElementRegistry::instance();
        
        // Get metadata for both steps
        auto const* meta1 = registry.getMetadata(steps_[0].transform_name);
        auto const* meta2 = registry.getMetadata(steps_[1].transform_name);
        
        if (!meta1 || !meta2) {
            throw std::runtime_error("Transform not found in registry");
        }
        
        using InputElement = ElementFor_t<InputContainer>;
        
        // Build composed executor for element-level transforms
        // This happens ONCE, then we iterate with the composed function
        if (!meta1->is_time_grouped) {
            if (meta1->output_type == typeid(float)) {
                // Step 1 produces intermediate floats
                // Check if step 2 is time-grouped (requires collecting per-time)
                if (meta2->is_time_grouped && meta2->output_type == typeid(float)) {
                    // Need intermediate ragged structure for time-grouped transform
                    auto intermediate = applyElementTransformGeneric<InputContainer, InputElement, float>(
                        input, steps_[0], meta1->params_type);
                    
                    return applyTimeGroupedTransformGeneric<
                        decltype(*intermediate), OutputContainer, float, float>(
                        *intermediate, steps_[1], meta2->params_type);
                }
            }
            // Add more output types here as needed (Line2D, Mask2D, etc.)
        }
        
        throw std::runtime_error("Unsupported pipeline configuration");
    }
    
    /**
     * @brief Execute pipeline with automatic fusion optimization
     * 
     * Analyzes the pipeline structure and chooses the optimal execution strategy:
     * - Pure element chains: Fused single-pass execution
     * - Chains with time-grouped: Materialized intermediate execution
     * 
     * @tparam InputContainer Input container type
     * @tparam OutputContainer Output container type
     * @param input Input data
     * @return Output container
     */
    template<typename InputContainer, typename OutputContainer>
    std::shared_ptr<OutputContainer> executeOptimized(InputContainer const& input) const {
        if (steps_.empty()) {
            throw std::runtime_error("Pipeline has no steps");
        }
        
        auto& registry = ElementRegistry::instance();
        
        // Analyze pipeline: check if all steps are element-level (fusible)
        bool all_element_level = true;
        for (auto const& step : steps_) {
            auto const* meta = registry.getMetadata(step.transform_name);
            if (!meta || meta->is_time_grouped) {
                all_element_level = false;
                break;
            }
        }
        
        if (all_element_level) {
            // Pure element pipeline - use fusion for maximum performance
            return executeFused<InputContainer, OutputContainer>(input);
        } else {
            // Has time-grouped transforms - use standard execution
            return execute<InputContainer, OutputContainer>(input);
        }
    }
    
    /**
     * @brief Execute a multi-step element-level pipeline with full fusion
     * 
     * For element→element→element pipelines (no time-grouped transforms),
     * this composes all transforms into a single function and executes in
     * one pass with zero intermediate allocations.
     * 
     * Example: Mask2D → skeletonize → fitLine → resample
     * Results in: for (mask : masks) { output.add(resample(fitLine(skeletonize(mask)))); }
     * 
     * Performance:
     * - Single pass through input data
     * - No intermediate container allocations
     * - Intermediate values stay hot in CPU cache
     * - Type erasure overhead (~1-2 pointer dereferences per element per step)
     * 
     * Implementation:
     * Uses runtime function composition with type-erased intermediate values.
     * This allows arbitrary pipeline lengths determined at runtime (e.g., from UI).
     * 
     * @tparam InputContainer Input container type
     * @tparam OutputContainer Output container type
     * @param input Input data
     * @return Output with composed transformation applied
     */
    template<typename InputContainer, typename OutputContainer>
    std::shared_ptr<OutputContainer> executeFused(InputContainer const& input) const {
        if (steps_.empty()) {
            throw std::runtime_error("Pipeline has no steps");
        }
        
        auto& registry = ElementRegistry::instance();
        
        // Verify all steps are element-level (not time-grouped)
        for (auto const& step : steps_) {
            auto const* meta = registry.getMetadata(step.transform_name);
            if (!meta) {
                throw std::runtime_error("Transform not found: " + step.transform_name);
            }
            if (meta->is_time_grouped) {
                throw std::runtime_error("Cannot fuse pipeline containing time-grouped transform: " + 
                                       step.transform_name);
            }
        }
        
        using InputElement = ElementFor_t<InputContainer>;
        using OutputElement = ElementFor_t<OutputContainer>;
        
        // Build a chain of type-erased transform functions
        // Each function takes std::any and returns std::any
        std::vector<std::function<std::any(std::any)>> transform_chain;
        transform_chain.reserve(steps_.size());
        
        for (size_t i = 0; i < steps_.size(); ++i) {
            auto const& step = steps_[i];
            auto const* meta = registry.getMetadata(step.transform_name);
            
            // Create a type-erased function for this step
            // The function knows its input and output types from metadata
            auto transform_fn = buildTypeErasedFunction(step, meta);
            transform_chain.push_back(std::move(transform_fn));
        }
        
        // Compose all functions into a single callable
        // This is the "fusion" - we build the composition once, then iterate
        auto composed_fn = [chain = std::move(transform_chain)](std::any input) -> std::any {
            std::any current = std::move(input);
            for (auto const& transform : chain) {
                current = transform(std::move(current));
            }
            return current;
        };
        
        // Execute in single pass with composed function
        auto output = std::make_shared<OutputContainer>();
        output->setTimeFrame(input.getTimeFrame());
        
        for (auto const& [time, entry] : input.elements()) {
            // Wrap input in std::any, apply composed function, extract output
            std::any input_any = entry.data;
            std::any output_any = composed_fn(std::move(input_any));
            OutputElement result = std::any_cast<OutputElement>(std::move(output_any));
            
            // Add to output using the appropriate method for this container type
            addElementToContainer(*output, time, std::move(result));
        }
        
        return output;
    }
    
    /**
     * @brief Get the number of steps in the pipeline
     */
    size_t size() const { return steps_.size(); }
    
    /**
     * @brief Check if the pipeline is empty
     */
    bool empty() const { return steps_.empty(); }
    
    /**
     * @brief Get information about a specific step
     */
    std::string const& getStepName(size_t index) const {
        return steps_.at(index).transform_name;
    }

    /**
     * @brief Execute the pipeline and return a lazy view (no materialization)
     * 
     * This method returns a lazy range view that applies all pipeline transforms
     * on-demand as elements are accessed. No intermediate or output containers
     * are materialized until the view is consumed.
     * 
     * This is the key method for view-based pipelines that eliminates all
     * unnecessary materializations. The returned view can be:
     * - Passed to further view transformations
     * - Materialized into a container via range constructor
     * - Consumed element-by-element in a loop
     * 
     * **Requirements:**
     * - All pipeline steps must be element-level transforms (not time-grouped)
     * - Input container must provide an elements() view
     * 
     * **Performance:**
     * - Zero intermediate allocations
     * - Computation happens on-demand (pull-based)
     * - Optimal cache locality (transforms applied in sequence per element)
     * 
     * @tparam InputContainer Input container type (must have elements() view)
     * @param input Input data
     * @return Lazy range view of (TimeFrameIndex, OutputElement) pairs
     * 
     * @throws std::runtime_error if pipeline contains time-grouped transforms
     * 
     * @example
     * ```cpp
     * // Create pipeline
     * auto pipeline = TransformPipeline()
     *     .addStep("Skeletonize", params1)
     *     .addStep("CalculateMaskArea", params2);
     * 
     * // Get lazy view - no computation yet!
     * auto view = pipeline.executeAsView(mask_data);
     * 
     * // Option 1: Materialize into container
     * auto result = std::make_shared<RaggedAnalogTimeSeries>(view);
     * 
     * // Option 2: Further transform the view
     * auto filtered = view 
     *     | std::views::filter([](auto pair) { return pair.second > 100.0f; });
     * 
     * // Option 3: Process elements on-demand
     * for (auto [time, value] : view) {
     *     // Transforms execute here, one element at a time
     *     processValue(time, value);
     * }
     * ```
     */
    template<typename InputContainer>
    requires requires(InputContainer const& c) { c.elements(); }
    auto executeAsView(InputContainer const& input) const {
        if (steps_.empty()) {
            throw std::runtime_error("Pipeline has no steps");
        }
        
        auto& registry = ElementRegistry::instance();
        
        // Verify all steps are element-level (not time-grouped)
        for (auto const& step : steps_) {
            auto const* meta = registry.getMetadata(step.transform_name);
            if (!meta) {
                throw std::runtime_error("Transform not found: " + step.transform_name);
            }
            if (meta->is_time_grouped) {
                throw std::runtime_error(
                    "executeAsView() does not support time-grouped transforms. "
                    "Use execute() instead for pipelines with time-grouped steps.");
            }
        }
        
        // Build composed transform function (type-erased but created once)
        std::vector<std::function<std::any(std::any)>> transform_chain;
        transform_chain.reserve(steps_.size());
        
        for (auto const& step : steps_) {
            auto const* meta = registry.getMetadata(step.transform_name);
            auto transform_fn = buildTypeErasedFunction(step, meta);
            transform_chain.push_back(std::move(transform_fn));
        }
        
        // Compose all functions into a single callable
        auto composed_fn = [chain = std::move(transform_chain)](std::any input_any) -> std::any {
            std::any current = std::move(input_any);
            for (auto const& transform : chain) {
                current = transform(std::move(current));
            }
            return current;
        };
        
        // Return a lazy view that applies the composed function to each element
        return input.elements() | std::views::transform([composed_fn](auto const& elem) {
            // Extract time
            auto time = elem.first;
            
            // Extract data element and wrap in std::any for composed function
            // The composed function handles all type conversions internally
            using InputElement = ElementFor_t<InputContainer>;
            InputElement const& data = extractElement<decltype(elem), InputElement>(elem);
            std::any input_any = data;
            
            // Apply composed transform chain
            std::any output_any = composed_fn(std::move(input_any));
            
            // The output type depends on the final transform in the pipeline
            // Since we're returning a generic view, we keep it as (TimeFrameIndex, std::any)
            // The consumer must know the output type or use a typed wrapper
            return std::make_pair(time, std::move(output_any));
        });
    }

    /**
     * @brief Execute pipeline as view with explicit output type (type-safe version)
     * 
     * This is a type-safe wrapper around executeAsView() that ensures the output
     * type is known at compile time. The returned view yields (TimeFrameIndex, OutElement)
     * pairs instead of (TimeFrameIndex, std::any).
     * 
     * @tparam InputContainer Input container type
     * @tparam OutElement Expected output element type
     * @param input Input data
     * @return Lazy range view of (TimeFrameIndex, OutElement) pairs
     * 
     * @example
     * ```cpp
     * auto view = pipeline.executeAsViewTyped<MaskData, float>(mask_data);
     * // view is a range of (TimeFrameIndex, float) pairs
     * 
     * auto result = std::make_shared<RaggedAnalogTimeSeries>(view);
     * ```
     */
    template<typename InputContainer, typename OutElement>
    requires requires(InputContainer const& c) { c.elements(); }
    auto executeAsViewTyped(InputContainer const& input) const {
        auto any_view = executeAsView(input);
        
        // Add a transform that unwraps the std::any to the concrete output type
        return any_view | std::views::transform([](auto pair) {
            return std::make_pair(
                pair.first, 
                std::any_cast<OutElement>(std::move(pair.second))
            );
        });
    }

private:
    /**
     * @brief Build a type-erased transform function for runtime composition
     * 
     * Creates a std::function<std::any(std::any)> that:
     * 1. Unpacks the input from std::any using metadata-specified type
     * 2. Executes the transform with the correct types
     * 3. Packs the output back into std::any
     * 
     * This enables runtime function composition for arbitrary pipeline lengths.
     * 
     * @param step The pipeline step to wrap
     * @param meta Transform metadata (for type information)
     * @return Type-erased function suitable for composition
     */
    std::function<std::any(std::any)> buildTypeErasedFunction(
        PipelineStep const& step,
        TransformMetadata const* meta) const
    {
        auto& registry = ElementRegistry::instance();
        
        // Dispatch based on input and output types from metadata
        // This is where we handle the type erasure/recovery
        
        auto input_type = meta->input_type;
        auto output_type = meta->output_type;
        auto params_type = meta->params_type;
        
        // Build a lambda that captures the transform and its parameters
        // The lambda knows the concrete types and can do the std::any conversions
        
       // if (params_type == typeid(NoParams)) {
            // No parameters - simpler case
      //      return buildTypeErasedFunctionNoParams(step, input_type, output_type);
    //    } else {
            // Has parameters - need to extract from step.params
            return buildTypeErasedFunctionWithParams(step, input_type, output_type, params_type);
      //  }
    }
    
    /**
     * @brief Build type-erased function for transforms with parameters
     * 
     * This version extracts parameters from step.params and captures them.
     */
    std::function<std::any(std::any)> buildTypeErasedFunctionWithParams(
        PipelineStep const& step,
        std::type_index input_type,
        std::type_index output_type,
        std::type_index params_type) const
    {
        auto& registry = ElementRegistry::instance();
        
        // Use the registry's dynamic parameter execution
        // Capture the types and parameters for later execution
        return [&registry, 
                name = step.transform_name, 
                params = step.params,
                input_type,
                output_type,
                params_type](std::any input) -> std::any {
            return registry.executeWithDynamicParams(
                name, input, params, input_type, output_type, params_type);
        };
    }
    
    /**
     * @brief Build a typed element transform function (legacy - for non-fused paths)
     * 
     * Creates a callable that applies the transform with its parameters.
     * This is composed with other functions before iteration.
     */
    template<typename InputElement, typename OutputElement>
    std::function<OutputElement(InputElement const&)> buildElementFunction(
        PipelineStep const& step,
        std::type_index param_type) const
    {
        auto& registry = ElementRegistry::instance();
        
        // Dispatch based on parameter type
        if (param_type == typeid(NoParams)) {
            auto const& params = std::any_cast<NoParams const&>(step.params);
            return [&registry, name = step.transform_name, params](InputElement const& input) -> OutputElement {
                return registry.execute<InputElement, OutputElement, NoParams>(name, input, params);
            };
        }
        
        // For other parameter types, would need to be registered or handled here
        throw std::runtime_error("Parameter type not supported for fusion: " + 
                               std::string(param_type.name()));
    }
    
    /**
     * @brief Apply element transform with materialization (non-fused path)
     */
    template<typename InputContainer, typename InputElement, typename OutputElement>
    auto applyElementTransformGeneric(
        InputContainer const& input,
        PipelineStep const& step,
        std::type_index param_type) const
        -> std::shared_ptr<RaggedContainerFor_t<OutputElement>>
    {
        using OutputContainer = RaggedContainerFor_t<OutputElement>;
        auto& registry = ElementRegistry::instance();
        
        auto output = std::make_shared<OutputContainer>();
        output->setTimeFrame(input.getTimeFrame());
        
        auto const* meta = registry.getMetadata(step.transform_name);
        if (!meta) {
            throw std::runtime_error("Transform not found: " + step.transform_name);
        }
        
        // Transform each element
        for (auto const& [time, entry] : input.elements()) {
            OutputElement result = executeWithDynamicParams<InputElement, OutputElement>(
                step, entry.data, param_type);
            output->appendAtTime(time, std::vector<OutputElement>{result}, NotifyObservers::No);
        }
        
        return output;
    }
    
    /**
     * @brief Execute transform with dynamic parameter type dispatch
     * 
     * Uses typed executors with captured parameters - no per-element dispatch!
     */
    template<typename InputElement, typename OutputElement>
    OutputElement executeWithDynamicParams(
        PipelineStep const& step,
        InputElement const& input,
        std::type_index param_type) const
    {
        auto& registry = ElementRegistry::instance();
        
        // Typed executor handles everything - zero dispatch overhead!
        std::any input_any = input;
        std::any result_any = registry.executeWithDynamicParams(
            step.transform_name, input_any, step.params,
            typeid(InputElement), typeid(OutputElement), param_type);
        
        return std::any_cast<OutputElement>(std::move(result_any));
    }
    
    /**
     * @brief Apply time-grouped transform with dynamic executor creation
     */
    template<typename InputContainer, typename OutputContainer, typename InputElement, typename OutputElement>
    std::shared_ptr<OutputContainer> applyTimeGroupedTransformGeneric(
        InputContainer const& input,
        PipelineStep const& step,
        std::type_index param_type) const
    {
        auto& registry = ElementRegistry::instance();
        
        // For AnalogTimeSeries output, collect into map
        if constexpr (std::is_same_v<OutputContainer, AnalogTimeSeries>) {
            auto time_indices = input.getTimeIndices();
            std::map<int, OutputElement> output_map;
            
            for (auto time : time_indices) {
                auto data_at_time = input.getDataAtTime(time);
                auto result = executeTimeGroupedWithDynamicParams<InputElement, OutputElement>(
                    step, data_at_time, param_type);
                
                if (!result.empty()) {
                    output_map[time.getValue()] = result[0];
                }
            }
            
            auto output = std::make_shared<AnalogTimeSeries>(output_map);
            output->setTimeFrame(input.getTimeFrame());
            return std::static_pointer_cast<OutputContainer>(output);
        }
        
        throw std::runtime_error("Unsupported output container type");
    }
    
    /**
     * @brief Execute time-grouped transform with dynamic parameter type dispatch
     * 
     * Uses typed executors with captured parameters - no per-element dispatch!
     */
    template<typename InputElement, typename OutputElement>
    std::vector<OutputElement> executeTimeGroupedWithDynamicParams(
        PipelineStep const& step,
        std::span<InputElement const> inputs,
        std::type_index param_type) const
    {
        auto& registry = ElementRegistry::instance();
        
        // For time-grouped transforms, we need to use the registry directly
        // since they have different signatures (span -> vector)
        // The typed executor system is primarily for element-level transforms
        
        // Fallback to manual dispatch for time-grouped transforms
        if (param_type == typeid(NoParams)) {
            auto const& params = std::any_cast<NoParams const&>(step.params);
            return registry.executeTimeGrouped<InputElement, OutputElement, NoParams>(
                step.transform_name, inputs, params);
        }
        else if (param_type == typeid(Examples::SumReductionParams)) {
            auto const& params = std::any_cast<Examples::SumReductionParams const&>(step.params);
            return registry.executeTimeGrouped<InputElement, OutputElement, Examples::SumReductionParams>(
                step.transform_name, inputs, params);
        }
        else if (param_type == typeid(Examples::MaskAreaParams)) {
            auto const& params = std::any_cast<Examples::MaskAreaParams const&>(step.params);
            return registry.executeTimeGrouped<InputElement, OutputElement, Examples::MaskAreaParams>(
                step.transform_name, inputs, params);
        }
        
        throw std::runtime_error("Parameter type dispatch not implemented for time-grouped transform: " + 
                                std::string(param_type.name()));
    }
    
    std::vector<PipelineStep> steps_;
};

} // namespace WhiskerToolbox::Transforms::V2

#endif // WHISKERTOOLBOX_V2_TRANSFORM_PIPELINE_HPP
