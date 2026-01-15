#ifndef WHISKERTOOLBOX_V2_TRANSFORM_PIPELINE_HPP
#define WHISKERTOOLBOX_V2_TRANSFORM_PIPELINE_HPP

#include "ContainerTraits.hpp"
#include "ContainerTransform.hpp"
#include "ContextAwareParams.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"
#include "DataManager/DataManagerTypes.hpp"
#include "ElementRegistry.hpp"
#include "PreProcessingRegistry.hpp"
#include "RangeReductionRegistry.hpp"
#include "TransformTypes.hpp"
#include "ValueProjectionTypes.hpp"
#include "ViewAdaptorTypes.hpp"

#include <any>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <type_traits>
#include <typeindex>
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
void addElementToContainer(Container & container, TimeFrameIndex time, Element && element) {
    // Check if container has appendAtTime (like RaggedAnalogTimeSeries)
    if constexpr (requires { container.appendAtTime(time, std::vector<Element>{}, NotifyObservers::No); }) {
        // Wrap single element in vector for appendAtTime
        container.appendAtTime(time, std::vector<Element>{std::forward<Element>(element)}, NotifyObservers::No);
    }
    // Check if container has addAtTime for single elements (like RaggedTimeSeries<T>)
    else if constexpr (requires { container.addAtTime(time, std::forward<Element>(element), NotifyObservers::No); }) {
        container.addAtTime(time, std::forward<Element>(element), NotifyObservers::No);
    } else {
        static_assert(always_false<Container>, "Container type does not support addAtTime or appendAtTime");
    }
}

// ============================================================================
// Pipeline Output Builder
// ============================================================================

/**
 * @brief Helper to build output containers efficiently
 * 
 * Handles both incremental addition (RaggedTimeSeries) and batch loading (AnalogTimeSeries).
 */
template<typename Container, typename Element>
class PipelineOutputBuilder {
public:
    PipelineOutputBuilder(std::shared_ptr<TimeFrame> tf)
        : tf_(tf) {
        if constexpr (use_incremental) {
            container_ = std::make_shared<Container>();
            if constexpr (requires { container_->setTimeFrame(tf); }) {
                container_->setTimeFrame(tf);
            }
        }
    }

    void add(TimeFrameIndex time, Element element) {
        if constexpr (use_incremental) {
            addElementToContainer(*container_, time, std::move(element));
        } else {
            times_.push_back(time);
            values_.push_back(std::move(element));
        }
    }

    std::shared_ptr<Container> finalize() {
        if constexpr (use_incremental) {
            return container_;
        } else {
            auto container = std::make_shared<Container>(std::move(values_),
                                                         std::move(times_));
            if constexpr (requires { container->setTimeFrame(tf_); }) {
                container->setTimeFrame(tf_);
            }
            return container;
        }
    }

private:
    // Detect incremental capabilities
    static constexpr bool has_add_at_time = requires(Container & c, TimeFrameIndex t, Element e) {
        c.addAtTime(t, e, NotifyObservers::No);
    };

    static constexpr bool has_append_at_time = requires(Container & c, TimeFrameIndex t, std::vector<Element> v) {
        c.appendAtTime(t, v, NotifyObservers::No);
    };

    // Prefer incremental if available, otherwise fallback to batch
    static constexpr bool use_incremental = has_add_at_time || has_append_at_time;

    std::shared_ptr<Container> container_;
    std::shared_ptr<TimeFrame> tf_;

    std::vector<TimeFrameIndex> times_;
    std::vector<Element> values_;
};

// ============================================================================
// Pipeline Step Definition
// ============================================================================

// Forward declare for ADL
struct PipelineStep;

// Default implementation declared but not defined
// RegisteredTransforms.hpp provides the definition
template<typename View>
void tryAllRegisteredPreprocessing(PipelineStep const &, View const &);

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
    mutable std::any params;// Type-erased parameters (mutable for preprocessing/caching)

    // Type-erased executors that know the correct parameter type
    // These are set when the step is added to the pipeline
    std::function<ElementVariant(ElementVariant const &, std::any const &)> element_executor;
    std::function<BatchVariant(BatchVariant const &, std::any const &)> time_grouped_executor;

    template<typename Params>
    PipelineStep(std::string name, Params p)
        : transform_name(std::move(name)),
          params(std::move(p)) {
        // Create type-erased executors that capture the parameter type
        auto & registry = ElementRegistry::instance();
        auto const * meta = registry.getMetadata(transform_name);

        if (meta && !meta->is_time_grouped) {
            // Element transform executor
            element_executor = [name = transform_name, p = this->params](
                                       ElementVariant const & input, std::any const &) -> ElementVariant {
                // This will be called with the correct types by the pipeline
                // The actual execution is deferred until we know InputElement and OutputElement
                return ElementVariant{};// Placeholder - will be replaced by specialized executor
            };
        } else if (meta && meta->is_time_grouped) {
            // Time-grouped transform executor
            time_grouped_executor = [name = transform_name, p = this->params](
                                            BatchVariant const & input, std::any const &) -> BatchVariant {
                return BatchVariant{};// Placeholder
            };
        }
    }

    PipelineStep(std::string name)
        : PipelineStep(std::move(name), NoParams{}) {}

    /**
     * @brief Try preprocessing using registry lookup
     * 
     * This is a compile-time template that gets instantiated with both
     * the View type and the Params type known. The Params type is discovered
     * by trying registered types from the preprocessing registry.
     * 
     * @tparam View The view type (known at call site)
     * @tparam Params The parameter type (tried from registry)
     */
    template<typename View, typename Params>
    bool tryPreprocessTyped(View const & view) const {
        // Check type without throwing
        if (params.type() != typeid(Params)) {
            return false;
        }

        // Cast to concrete type (non-const since we're modifying it)
        auto & params_ref = std::any_cast<Params &>(params);

        // Check if this type has preprocess() method that's callable with this view type
        // The requires constraint on the preprocess method ensures type safety
        if constexpr (requires { params_ref.preprocess(view); }) {
            // Check for idempotency guard
            if constexpr (requires { params_ref.isPreprocessed(); }) {
                if (!params_ref.isPreprocessed()) {
                    params_ref.preprocess(view);
                }
            } else {
                params_ref.preprocess(view);
            }
        }

        return true;
    }

    /**
     * @brief Main preprocessing entry point
     * 
     * Calls free function that can be overloaded in RegisteredTransforms.hpp
     */
    template<typename View>
    void maybePreprocess(View const & view) const {
        // Call ADL-found free function
        // Default version does nothing, RegisteredTransforms.hpp provides the real version
        tryAllRegisteredPreprocessing(*this, view);
    }

private:
public:
    /**
     * @brief Create element executor for specific types
     * 
     * This template method creates a properly typed executor that can be called
     * from the pipeline without knowing the parameter type.
     */
    template<typename InputElement, typename OutputElement, typename Params>
    void createElementExecutor() {
        auto & registry = ElementRegistry::instance();
        element_executor = [&registry, name = transform_name, p = std::any_cast<Params>(params)](
                                   ElementVariant const & input_variant, std::any const &) -> ElementVariant {
            // Extract input from variant
            if (auto const * input = std::get_if<InputElement>(&input_variant)) {
                OutputElement result = registry.execute<InputElement, OutputElement, Params>(
                        name, *input, p);
                return ElementVariant{result};
            }
            throw std::runtime_error("Input variant does not hold expected type: " + std::string(typeid(InputElement).name()));
        };
    }

    /**
     * @brief Create time-grouped executor for specific types
     */
    template<typename InputElement, typename OutputElement, typename Params>
    void createTimeGroupedExecutor() {
        auto & registry = ElementRegistry::instance();
        time_grouped_executor = [&registry, name = transform_name, p = std::any_cast<Params>(params)](
                                        BatchVariant const & input_batch, std::any const &) -> BatchVariant {
            if (auto const * input_vec = std::get_if<std::vector<InputElement>>(&input_batch)) {
                std::span<InputElement const> input_span(*input_vec);
                std::vector<OutputElement> result = registry.executeTimeGrouped<InputElement, OutputElement, Params>(
                        name, input_span, p);
                return BatchVariant{result};
            }
            throw std::runtime_error("Input batch does not hold expected type: " + std::string(typeid(std::vector<InputElement>).name()));
        };
    }
};

// ============================================================================
// Batch Variant Helpers
// ============================================================================

void pushToBatch(BatchVariant & batch, ElementVariant const & element);

BatchVariant initBatchFromElement(ElementVariant const & element);

size_t getBatchSize(BatchVariant const & batch);

void clearBatch(BatchVariant & batch);

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
    TransformPipeline & addStep(std::string const & transform_name, Params params) {
        steps_.emplace_back(transform_name, std::move(params));
        return *this;
    }

    /**
     * @brief Add a transform step with no parameters
     */
    TransformPipeline & addStep(std::string const & transform_name) {
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
    TransformPipeline & addStep(PipelineStep step) {
        steps_.push_back(std::move(step));
        return *this;
    }

    // ========================================================================
    // Terminal Range Reduction
    // ========================================================================

    /**
     * @brief Set a terminal range reduction for the pipeline
     *
     * A range reduction is applied after all element transforms to collapse
     * a range of elements into a scalar. This is used with bindReducer()
     * to create reducer functions for sorting, partitioning, etc.
     *
     * @tparam IntermediateElement Element type after all element transforms
     * @tparam Scalar Output scalar type
     * @tparam Params Parameter type for the reduction
     * @param reduction_name Name of the registered range reduction
     * @param params Parameters for the reduction
     * @return Reference to this pipeline for chaining
     *
     * @example
     * ```cpp
     * auto pipeline = TransformPipeline()
     *     .addStep("NormalizeEventTime", NormalizeTimeParams{})
     *     .setRangeReduction<NormalizedEvent, float>("FirstPositiveLatency",
     *                                                 FirstPositiveLatencyParams{});
     * ```
     */
    template<typename IntermediateElement, typename Scalar, typename Params>
    TransformPipeline & setRangeReduction(std::string const & reduction_name, Params params) {
        range_reduction_ = RangeReductionStep{reduction_name, std::move(params)};
        range_reduction_->input_type = typeid(IntermediateElement);
        range_reduction_->output_type = typeid(Scalar);
        return *this;
    }

    /**
     * @brief Set a stateless terminal range reduction
     */
    template<typename IntermediateElement, typename Scalar>
    TransformPipeline & setRangeReduction(std::string const & reduction_name) {
        return setRangeReduction<IntermediateElement, Scalar, NoReductionParams>(
            reduction_name, NoReductionParams{});
    }

    /**
     * @brief Set a terminal range reduction with type-erased parameters
     *
     * This is used when loading pipelines from JSON where parameter types
     * are resolved at runtime via the registry. The input/output types will
     * be determined from the reduction's metadata.
     *
     * @param reduction_name Name of the registered range reduction
     * @param params Type-erased parameters (must match the reduction's expected type)
     * @return Reference to this pipeline for chaining
     */
    TransformPipeline & setRangeReductionErased(std::string const & reduction_name,
                                                 std::any params) {
        auto & registry = RangeReductionRegistry::instance();
        auto const * meta = registry.getMetadata(reduction_name);

        range_reduction_ = RangeReductionStep{};
        range_reduction_->reduction_name = reduction_name;
        range_reduction_->params = std::move(params);

        if (meta) {
            range_reduction_->input_type = meta->input_type;
            range_reduction_->output_type = meta->output_type;
            range_reduction_->params_type = meta->params_type;
        }

        return *this;
    }

    /**
     * @brief Check if the pipeline has a terminal range reduction
     */
    [[nodiscard]] bool hasRangeReduction() const noexcept {
        return range_reduction_.has_value();
    }

    /**
     * @brief Get the range reduction step (if set)
     */
    [[nodiscard]] std::optional<RangeReductionStep> const & getRangeReduction() const noexcept {
        return range_reduction_;
    }

    /**
     * @brief Clear the terminal range reduction
     */
    void clearRangeReduction() noexcept {
        range_reduction_.reset();
    }

private:
    struct Segment {
        bool is_element_wise;
        std::vector<size_t> step_indices;// Indices into steps_
        std::type_index input_type = typeid(void);
        std::type_index output_type = typeid(void);

        // For fused element segment
        std::function<ElementVariant(ElementVariant)> fused_fn;
    };

    /**
     * @brief Internal optimized execution for pure element-wise segments
     */
    template<typename InputContainer, typename OutputContainer>
    std::shared_ptr<OutputContainer> executeFusedImpl(
            InputContainer const & input,
            std::function<ElementVariant(ElementVariant)> const & fused_fn) const {

        using InputElement = ElementFor_t<InputContainer>;
        using OutputElement = ElementFor_t<OutputContainer>;

        PipelineOutputBuilder<OutputContainer, OutputElement> builder(input.getTimeFrame());

        for (auto const & elem: input.elements()) {
            auto time = elem.first;

            // Extract data using generic helper
            InputElement const & data = extractElement<decltype(elem), InputElement>(elem);

            // Apply fused transform
            // Input is implicitly converted to ElementVariant (if compatible)
            // Output is ElementVariant, need to extract OutputElement
            ElementVariant output_var = fused_fn(ElementVariant{data});

            if (auto const * result = std::get_if<OutputElement>(&output_var)) {
                builder.add(time, *result);
            } else {
                throw std::runtime_error("Fused execution produced unexpected type: " +
                                         std::string(output_var.index() == std::variant_npos ? "empty" : "wrong type"));
            }
        }

        return builder.finalize();
    }

    /**
     * @brief Internal execution logic with known types
     */
    template<typename InputContainer, typename OutputContainer>
    std::shared_ptr<OutputContainer> executeImpl(InputContainer const & input, std::vector<Segment> const & segments) const {
        // Optimization: If pipeline is pure element-wise (single segment), use fused execution
        if (segments.size() == 1 && segments[0].is_element_wise) {
            return executeFusedImpl<InputContainer, OutputContainer>(input, segments[0].fused_fn);
        }

        auto & registry = ElementRegistry::instance();

        // 3. Prepare output builder
        PipelineOutputBuilder<OutputContainer, ElementFor_t<OutputContainer>> builder(input.getTimeFrame());

        using InputElement = ElementFor_t<InputContainer>;
        using OutputElement = ElementFor_t<OutputContainer>;

        // 4. Execute pipeline time-point by time-point with buffering
        BatchVariant time_buffer;
        bool buffer_initialized = false;

        TimeFrameIndex current_time{0};
        bool first_element = true;

        // Helper to process the buffered data for a single time point
        auto process_buffer = [&](TimeFrameIndex t) {
            if (!buffer_initialized || getBatchSize(time_buffer) == 0) return;

            BatchVariant current_batch = std::move(time_buffer);
            buffer_initialized = false;

            // Determine which segment to start with
            size_t start_seg_idx = 0;
            if (!segments.empty() && segments[0].is_element_wise) {
                start_seg_idx = 1;// Already processed by S0 during collection
            }

            // Pass through remaining segments
            for (size_t i = start_seg_idx; i < segments.size(); ++i) {
                auto & seg = segments[i];

                if (seg.is_element_wise) {
                    // Map vector -> vector (element-wise)
                    BatchVariant next_batch;
                    bool next_initialized = false;

                    std::visit([&](auto const & vec) {
                        for (auto const & elem: vec) {
                            ElementVariant res = seg.fused_fn(ElementVariant{elem});
                            if (!next_initialized) {
                                next_batch = initBatchFromElement(res);
                                next_initialized = true;
                            } else {
                                pushToBatch(next_batch, res);
                            }
                        }
                    },
                               current_batch);

                    current_batch = std::move(next_batch);
                } else {
                    // Grouped transform
                    auto const & step = steps_[seg.step_indices[0]];
                    auto const * meta = registry.getMetadata(step.transform_name);

                    current_batch = registry.executeTimeGroupedWithDynamicParams(
                            step.transform_name,
                            current_batch,
                            step.params,
                            seg.input_type,
                            seg.output_type,
                            meta->params_type);
                }
            }

            // Write to output
            std::visit([&](auto const & vec) {
                using VecT = std::decay_t<decltype(vec)>;
                using ValT = typename VecT::value_type;

                if constexpr (std::is_same_v<ValT, OutputElement>) {
                    for (auto const & val: vec) {
                        builder.add(t, val);
                    }
                } else {
                    throw std::runtime_error("Pipeline output type mismatch");
                }
            },
                       current_batch);
        };

        // Iterate input elements using the view
        for (auto const & elem: input.elements()) {
            auto time = elem.first;

            if (!first_element && time != current_time) {
                process_buffer(current_time);
            }

            current_time = time;
            first_element = false;

            // Extract data using generic helper
            InputElement const & data = extractElement<decltype(elem), InputElement>(elem);
            ElementVariant val{data};

            // If first segment is element-wise, apply it immediately before buffering
            if (!segments.empty() && segments[0].is_element_wise) {
                val = segments[0].fused_fn(std::move(val));
            }

            if (!buffer_initialized) {
                time_buffer = initBatchFromElement(val);
                buffer_initialized = true;
            } else {
                pushToBatch(time_buffer, val);
            }
        }

        // Process last buffer
        if (buffer_initialized && getBatchSize(time_buffer) > 0) {
            process_buffer(current_time);
        }

        return builder.finalize();
    }

public:
    /**
     * @brief Execute pipeline and return variant result (Generalist)
     * 
     * Determines output container type at runtime based on pipeline steps
     * and input raggedness. This is the primary entry point for runtime-composed
     * pipelines (e.g. from UI) where the output type is not known at compile time.
     * 
     * Uses a buffered execution model that handles transitions between element-wise
     * and time-grouped transforms.
     * 
     * @see executeOptimized For compile-time known types with automatic optimization
     * @see executeFused For high-performance pure element-wise pipelines
     */
    template<typename InputContainer>
    DataTypeVariant execute(InputContainer const & input) const;

    /**
     * @brief Execute pipeline with automatic fusion optimization (Dispatcher)
     * 
     * Analyzes the pipeline structure and chooses the optimal execution strategy:
     * - Pure element chains: Fused single-pass execution (via executeFused)
     * - Chains with time-grouped: Materialized intermediate execution (via execute)
     * 
     * Use this when the output type is known at compile time.
     * 
     * @tparam InputContainer Input container type
     * @tparam OutputContainer Output container type
     * @param input Input data
     * @return Output container
     * 
     * @see executeFused The specialized implementation for pure element pipelines
     * @see execute The general implementation handling time-grouped transforms
     */
    template<typename InputContainer, typename OutputContainer>
    std::shared_ptr<OutputContainer> executeOptimized(InputContainer const & input) const {
        if (steps_.empty()) {
            throw std::runtime_error("Pipeline has no steps");
        }

        auto & registry = ElementRegistry::instance();

        // Analyze pipeline: check if all steps are element-level (fusible)
        bool all_element_level = true;
        for (auto const & step: steps_) {
            auto const * meta = registry.getMetadata(step.transform_name);
            if (!meta || meta->is_time_grouped) {
                all_element_level = false;
                break;
            }
        }

        if (all_element_level) {
            // Pure element pipeline - use fusion for maximum performance
            return executeFused<InputContainer, OutputContainer>(input);
        } else {
            // Has time-grouped transforms - use standard execution with variant
            auto result_variant = execute<InputContainer>(input);

            // Unwrap the variant to get the typed result
            auto result = std::get<std::shared_ptr<OutputContainer>>(result_variant);
            return result;
        }
    }

    /**
     * @brief Execute a multi-step element-level pipeline with full fusion (Specialist)
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
     * 
     * @see executeOptimized Automatic selection of this method
     * @see executeAsView For lazy evaluation without materialization
     */
    template<typename InputContainer, typename OutputContainer>
    std::shared_ptr<OutputContainer> executeFused(InputContainer const & input) const {
        if (steps_.empty()) {
            throw std::runtime_error("Pipeline has no steps");
        }

        auto & registry = ElementRegistry::instance();

        // Preprocessing phase: Allow params to compute statistics/allocate buffers
        // Automatically tries all registered parameter types
        auto view = input.elements();
        for (auto const & step: steps_) {
            step.maybePreprocess(view);
        }

        std::vector<std::function<ElementVariant(ElementVariant)>> transform_chain;

        // Verify all steps are element-level (not time-grouped)
        for (auto const & step: steps_) {
            auto const * meta = registry.getMetadata(step.transform_name);

            // Create a type-erased function for this step
            // The function knows its input and output types from metadata
            auto transform_fn = buildTypeErasedFunction(step, meta);
            transform_chain.push_back(std::move(transform_fn));
        }

        // Compose all functions into a single callable
        // This is the "fusion" - we build the composition once, then iterate
        auto composed_fn = [chain = std::move(transform_chain)](ElementVariant input) -> ElementVariant {
            ElementVariant current = std::move(input);
            for (auto const & transform: chain) {
                current = transform(std::move(current));
            }
            return current;
        };

        return executeFusedImpl<InputContainer, OutputContainer>(input, composed_fn);
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
    std::string const & getStepName(size_t index) const {
        return steps_.at(index).transform_name;
    }

    /**
     * @brief Get the step at a specific index
     */
    PipelineStep const & getStep(size_t index) const {
        return steps_.at(index);
    }

    /**
     * @brief Check if any step has context-aware parameters
     *
     * Returns true if any step's parameters satisfy TrialContextAwareParams.
     * This is used to determine whether to use bindToView or bindToViewWithContext.
     */
    [[nodiscard]] bool hasContextAwareSteps() const noexcept {
        // This is a runtime check that requires trying the context injection
        // We rely on the type system to detect this at compile time in the bind methods
        return false;  // Placeholder - actual detection happens at bind time
    }

    /**
     * @brief Check if the pipeline contains only element-level transforms
     *
     * Returns false if any step is a time-grouped transform.
     */
    [[nodiscard]] bool isElementWiseOnly() const {
        if (steps_.empty()) return true;

        auto & registry = ElementRegistry::instance();
        for (auto const & step : steps_) {
            auto const * meta = registry.getMetadata(step.transform_name);
            if (!meta || meta->is_time_grouped) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Execute pipeline on a generic view (View-Based Entry Point)
     * 
     * Allows executing the pipeline on any range that yields (TimeFrameIndex, InputElement) pairs.
     * This is essential for multi-input pipelines where the input is a zipped view (e.g. RaggedZipView).
     * 
     * @tparam InputElement The type of the data element in the view (e.g. Mask2D, tuple<Line2D, Point2D>)
     * @tparam View The range type
     * @param view The input view
     * @return Lazy range view of (TimeFrameIndex, ElementVariant) pairs
     */
    template<typename InputElement, std::ranges::input_range View>
    auto executeFromView(View && view) const {
        if (steps_.empty()) {
            throw std::runtime_error("Pipeline has no steps");
        }

        auto & registry = ElementRegistry::instance();

        // Preprocessing phase: Allow params to compute statistics/allocate buffers
        // Automatically tries all registered parameter types
        for (auto const & step: steps_) {
            step.maybePreprocess(view);
        }

        // Verify all steps are element-level (not time-grouped)
        for (auto const & step: steps_) {
            auto const * meta = registry.getMetadata(step.transform_name);
            if (!meta) {
                throw std::runtime_error("Transform not found: " + step.transform_name);
            }
            if (meta->is_time_grouped) {
                throw std::runtime_error(
                        "executeFromView() does not support time-grouped transforms. "
                        "Use execute() instead for pipelines with time-grouped steps.");
            }
        }

        // Build composed transform function (type-erased but created once)
        std::vector<std::function<ElementVariant(ElementVariant)>> transform_chain;
        // --- HEAD-TAIL EXECUTION STRATEGY ---
        // We split the pipeline into Head (first step) and Tail (rest).
        // Head handles the conversion from InputElement (which might be a tuple) to ElementVariant.
        // Tail handles the standard ElementVariant -> ElementVariant chain.

        auto const & head_step = steps_.front();
        auto const * head_meta = registry.getMetadata(head_step.transform_name);

        // Build Head Function: InputElement -> ElementVariant
        auto head_fn = [this, step = head_step, meta = head_meta](InputElement const & input) -> ElementVariant {
            auto & reg = ElementRegistry::instance();
            // Wrap input in std::any to pass through the generic interface
            std::any input_any{input};
            return reg.executeWithDynamicParamsAny(
                    step.transform_name,
                    input_any,
                    step.params,
                    std::type_index(typeid(InputElement)),// Use actual input type
                    meta->output_type,
                    meta->params_type);
        };

        // Build Tail Chain: ElementVariant -> ElementVariant
        std::vector<std::function<ElementVariant(ElementVariant)>> tail_chain;
        tail_chain.reserve(steps_.size() - 1);

        for (size_t i = 1; i < steps_.size(); ++i) {
            auto const & step = steps_[i];
            auto const * meta = registry.getMetadata(step.transform_name);
            tail_chain.push_back(buildTypeErasedFunction(step, meta));
        }

        // Compose Head and Tail
        auto composed_fn = [head = std::move(head_fn), tail = std::move(tail_chain)](InputElement const & input) -> ElementVariant {
            // Execute Head
            ElementVariant current = head(input);

            // Execute Tail
            for (auto const & transform: tail) {
                current = transform(std::move(current));
            }
            return current;
        };

        // Return a lazy view that applies the composed function to each element
        return std::forward<View>(view) | std::views::transform([composed_fn](auto const & elem) {
                   // Extract time
                   auto time = elem.first;

                   // Extract data element (second part of pair)
                   auto const & raw_data = std::get<1>(elem);

                   // Generic unwrapping helper
                   // Handles:
                   // 1. Direct match (T -> T)
                   // 2. DataEntry wrapper (DataEntry<T> -> T)
                   // 3. Tuple (for multi-input) - passed as is
                   auto const & get_input_ref = [](auto const & val) -> InputElement const & {
                       if constexpr (std::is_convertible_v<decltype(val), InputElement const &>) {
                           return val;
                       } else if constexpr (requires { val.data; }) {
                           // Try unwrapping .data (e.g. DataEntry<T>)
                           if constexpr (std::is_convertible_v<decltype(val.data), InputElement const &>) {
                               return val.data;
                           } else {
                               // If .data exists but isn't convertible, we can't do much.
                               // This might happen if InputElement is wrong.
                               // Return val and let compiler error on mismatch.
                               return (InputElement const &) val;
                           }
                       } else {
                           // Fallback: force cast/conversion and let compiler error if invalid
                           return (InputElement const &) val;
                       }
                   };

                   // Apply composed transform chain (Head + Tail)
                   ElementVariant output_any = composed_fn(get_input_ref(raw_data));

                   return std::make_pair(time, std::move(output_any));
               });
    }

    /**
     * @brief Execute the pipeline and return a lazy view (Lazy Evaluator)
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
     * @see executeAsViewTyped For type-safe lazy evaluation
     * @see executeFused For eager evaluation with similar performance characteristics
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
        requires requires(InputContainer const & c) { c.elements(); }
    auto executeAsView(InputContainer const & input) const {
        using InputElement = ElementFor_t<InputContainer>;
        return executeFromView<InputElement>(input.elements());
    }

    /**
     * @brief Execute pipeline as view with explicit output type (Type-Safe Lazy Evaluator)
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
     * @see executeAsView For the underlying generic implementation
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
        requires requires(InputContainer const & c) { c.elements(); }
    auto executeAsViewTyped(InputContainer const & input) const {
        auto any_view = executeAsView(input);

        // Add a transform that unwraps the variant to the concrete output type
        return any_view | std::views::transform([](auto pair) {
                   return std::make_pair(
                           pair.first,
                           std::get<OutElement>(std::move(pair.second)));
               });
    }

private:
    /**
     * @brief Build a type-erased transform function for runtime composition
     * 
     * Creates a std::function<ElementVariant(ElementVariant)> that:
     * 1. Unpacks the input from variant using metadata-specified type
     * 2. Executes the transform with the correct types
     * 3. Packs the output back into variant
     * 
     * This enables runtime function composition for arbitrary pipeline lengths.
     * 
     * @param step The pipeline step to wrap
     * @param meta Transform metadata (for type information)
     * @return Type-erased function suitable for composition
     */
    std::function<ElementVariant(ElementVariant)> buildTypeErasedFunction(
            PipelineStep const & step,
            TransformMetadata const * meta) const;

    /**
     * @brief Build type-erased function for transforms with parameters
     * 
     * This version extracts parameters from step.params and captures them.
     */
    std::function<ElementVariant(ElementVariant)> buildTypeErasedFunctionWithParams(
            PipelineStep const & step,
            std::type_index input_type,
            std::type_index output_type,
            std::type_index params_type) const;

    /**
     * @brief Execute transform with dynamic parameter type dispatch
     * 
     * Uses typed executors with captured parameters - no per-element dispatch!
     */
    template<typename InputElement, typename OutputElement>
    OutputElement executeWithDynamicParams(
            PipelineStep const & step,
            InputElement const & input,
            std::type_index param_type) const {
        auto & registry = ElementRegistry::instance();

        // Typed executor handles everything - zero dispatch overhead!
        ElementVariant input_var{input};
        ElementVariant result_var = registry.executeWithDynamicParams(
                step.transform_name, input_var, step.params,
                typeid(InputElement), typeid(OutputElement), param_type);

        return std::get<OutputElement>(std::move(result_var));
    }

    /**
     * @brief Execute time-grouped transform with dynamic parameter type dispatch
     * 
     * Uses typed executors with captured parameters - no per-element dispatch!
     */
    template<typename InputElement, typename OutputElement>
    std::vector<OutputElement> executeTimeGroupedWithDynamicParams(
            PipelineStep const & step,
            std::span<InputElement const> inputs,
            std::type_index param_type) const {
        auto & registry = ElementRegistry::instance();

        // Use the registry's dynamic parameter execution for time-grouped transforms
        // This uses the registered factories to create a typed executor on the fly
        std::vector<InputElement> input_vec(inputs.begin(), inputs.end());
        BatchVariant input_batch{std::move(input_vec)};

        BatchVariant result_batch = registry.executeTimeGroupedWithDynamicParams(
                step.transform_name, input_batch, step.params,
                typeid(InputElement), typeid(OutputElement), param_type);

        return std::get<std::vector<OutputElement>>(std::move(result_batch));
    }

    std::vector<PipelineStep> steps_;

    /// Optional terminal range reduction step
    std::optional<RangeReductionStep> range_reduction_;
};

// ============================================================================
// View Adaptor and Reducer Binding Methods
// ============================================================================

/**
 * @brief Bind a pipeline to produce a view adaptor function
 *
 * Creates a function that transforms a span of input elements into a vector
 * of output elements by applying all pipeline steps. The returned adaptor
 * can be used directly or composed with other operations.
 *
 * **Requirements:**
 * - Pipeline must have at least one step
 * - All steps must be element-level transforms (not time-grouped)
 * - Pipeline must not have context-aware transforms (use bindToViewWithContext)
 *
 * @tparam InElement Input element type (e.g., EventWithId)
 * @tparam OutElement Output element type (e.g., NormalizedEvent)
 * @param pipeline The transform pipeline to bind
 * @return ViewAdaptorFn that transforms span<InElement> → vector<OutElement>
 *
 * @throws std::runtime_error if pipeline is empty or contains time-grouped transforms
 *
 * @see bindToViewWithContext For context-aware transforms
 *
 * @example
 * ```cpp
 * auto pipeline = TransformPipeline()
 *     .addStep("ScaleValue", ScaleParams{.factor = 2.0f});
 * 
 * auto adaptor = bindToView<TimeValuePoint, TimeValuePoint>(pipeline);
 * auto transformed = adaptor(input_span);
 * ```
 */
template<typename InElement, typename OutElement>
ViewAdaptorFn<InElement, OutElement> bindToView(TransformPipeline const & pipeline);

/**
 * @brief Bind a pipeline with context-aware transforms to produce a factory
 *
 * Creates a factory function that takes TrialContext and returns a view adaptor.
 * The context is injected into any context-aware parameters in the pipeline
 * before creating the adaptor.
 *
 * @tparam InElement Input element type
 * @tparam OutElement Output element type
 * @param pipeline The transform pipeline to bind
 * @return ViewAdaptorFactory that produces adaptors from context
 *
 * @example
 * ```cpp
 * auto pipeline = TransformPipeline()
 *     .addStep("NormalizeEventTime", NormalizeTimeParams{});
 * 
 * auto factory = bindToViewWithContext<EventWithId, NormalizedEvent>(pipeline);
 * 
 * for (size_t i = 0; i < gather_result.size(); ++i) {
 *     TrialContext ctx{.alignment_time = gather_result.intervalAt(i).start};
 *     auto adaptor = factory(ctx);
 *     auto normalized = adaptor(span_from_view(gather_result[i]->view()));
 * }
 * ```
 */
template<typename InElement, typename OutElement>
ViewAdaptorFactory<InElement, OutElement> bindToViewWithContext(TransformPipeline const & pipeline);

/**
 * @brief Bind a pipeline with reduction to produce a reducer function
 *
 * Creates a function that transforms a span of input elements through the
 * pipeline steps and then applies the terminal range reduction to produce
 * a scalar.
 *
 * **Requirements:**
 * - Pipeline must have setRangeReduction() called
 * - All steps must be element-level transforms
 * - Pipeline must not have context-aware transforms (use bindReducerWithContext)
 *
 * @tparam InElement Input element type
 * @tparam Scalar Output scalar type
 * @param pipeline The transform pipeline with range reduction
 * @return ReducerFn that transforms span<InElement> → Scalar
 *
 * @throws std::runtime_error if pipeline has no range reduction
 *
 * @example
 * ```cpp
 * auto pipeline = TransformPipeline()
 *     .addStep("ScaleValue", ScaleParams{.factor = 1.0f})
 *     .setRangeReduction<TimeValuePoint, float>("MaxValue");
 * 
 * auto reducer = bindReducer<TimeValuePoint, float>(pipeline);
 * float max_val = reducer(input_span);
 * ```
 */
template<typename InElement, typename Scalar>
ReducerFn<InElement, Scalar> bindReducer(TransformPipeline const & pipeline);

/**
 * @brief Bind a pipeline with context-aware transforms and reduction
 *
 * Creates a factory that produces reducers with context injected.
 *
 * @tparam InElement Input element type
 * @tparam Scalar Output scalar type
 * @param pipeline The transform pipeline with range reduction
 * @return ReducerFactory that produces reducers from context
 *
 * @example
 * ```cpp
 * auto pipeline = TransformPipeline()
 *     .addStep("NormalizeEventTime", NormalizeTimeParams{})
 *     .setRangeReduction<NormalizedEvent, float>("FirstPositiveLatency");
 * 
 * auto factory = bindReducerWithContext<EventWithId, float>(pipeline);
 * 
 * auto latencies = gather_result.transform([&](auto const& trial, size_t idx) {
 *     TrialContext ctx{.alignment_time = gather_result.intervalAt(idx).start};
 *     return factory(ctx)(span_from_view(trial->view()));
 * });
 * ```
 */
template<typename InElement, typename Scalar>
ReducerFactory<InElement, Scalar> bindReducerWithContext(TransformPipeline const & pipeline);


// ============================================================================
// Value Projection Binding Methods
// ============================================================================

/**
 * @brief Bind a pipeline to produce a value projection function
 *
 * Creates a function that transforms a single input element into a scalar value
 * by applying all pipeline steps. Unlike ViewAdaptorFn which processes batches,
 * this operates on individual elements.
 *
 * **Value Projection Pattern:**
 * Instead of creating intermediate element types (e.g., NormalizedEvent),
 * value projections compute only the derived value (e.g., float for normalized time).
 * The source element retains identity information (EntityId, etc.) that can be
 * accessed directly without duplication.
 *
 * **Requirements:**
 * - Pipeline must have at least one step
 * - All steps must be element-level transforms (not time-grouped)
 * - Pipeline must not have context-aware transforms (use bindValueProjectionWithContext)
 * - Final output type must match Value
 *
 * @tparam InElement Input element type (e.g., EventWithId)
 * @tparam Value Output value type (e.g., float for normalized time)
 * @param pipeline The transform pipeline to bind
 * @return ValueProjectionFn that transforms InElement → Value
 *
 * @throws std::runtime_error if pipeline is empty or contains unsupported transforms
 *
 * @see bindValueProjectionWithContext For context-aware transforms
 * @see ValueProjectionTypes.hpp For type definitions
 *
 * @example
 * ```cpp
 * auto pipeline = TransformPipeline()
 *     .addStep("ScaleValue", ScaleParams{.factor = 2.0f});
 *
 * auto projection = bindValueProjection<TimeValuePoint, float>(pipeline);
 *
 * for (auto const& sample : view) {
 *     float scaled = projection(sample);
 *     // sample.time() still accessible from source
 * }
 * ```
 */
template<typename InElement, typename Value>
ValueProjectionFn<InElement, Value> bindValueProjection(TransformPipeline const & pipeline);

/**
 * @brief Bind a pipeline with context-aware transforms to produce a value projection factory
 *
 * Creates a factory function that takes TrialContext and returns a value projection.
 * The context is injected into any context-aware parameters in the pipeline
 * before creating the projection.
 *
 * **Primary Use Case: Raster Plots**
 * ```cpp
 * auto pipeline = TransformPipeline()
 *     .addStep("NormalizeEventTimeValue", NormalizeTimeParams{});
 *
 * auto factory = bindValueProjectionWithContext<EventWithId, float>(pipeline);
 *
 * for (size_t i = 0; i < gather_result.size(); ++i) {
 *     auto ctx = gather_result.buildContext(i);
 *     auto projection = factory(ctx);
 *
 *     for (auto const& event : gather_result[i]->view()) {
 *         float norm_time = projection(event);  // Computed value
 *         EntityId id = event.id();             // From source element
 *         draw(norm_time, i, id);
 *     }
 * }
 * ```
 *
 * @tparam InElement Input element type
 * @tparam Value Output value type
 * @param pipeline The transform pipeline to bind
 * @return ValueProjectionFactory that produces projections from context
 */
template<typename InElement, typename Value>
ValueProjectionFactory<InElement, Value> bindValueProjectionWithContext(TransformPipeline const & pipeline);


/**
 * @brief Execute pipeline on variant input
 */
DataTypeVariant executePipeline(DataTypeVariant const & input, TransformPipeline const & pipeline);


extern template DataTypeVariant TransformPipeline::execute<RaggedAnalogTimeSeries>(RaggedAnalogTimeSeries const &) const;
extern template DataTypeVariant TransformPipeline::execute<AnalogTimeSeries>(AnalogTimeSeries const &) const;
extern template DataTypeVariant TransformPipeline::execute<MaskData>(MaskData const &) const;
extern template DataTypeVariant TransformPipeline::execute<LineData>(LineData const &) const;
extern template DataTypeVariant TransformPipeline::execute<PointData>(PointData const &) const;

// ============================================================================
// Template Implementations: View Adaptor and Reducer Binding
// ============================================================================

namespace detail {

/**
 * @brief Build a composed transform function from pipeline steps
 *
 * Creates a function that applies all pipeline steps in sequence.
 * Used internally by bindToView and related functions.
 *
 * @param pipeline The pipeline to build from
 * @return Composed function: InElement → OutElement
 */
template<typename InElement, typename OutElement>
auto buildComposedTransformFn(TransformPipeline const & pipeline) {
    if (pipeline.empty()) {
        throw std::runtime_error("Pipeline has no steps");
    }

    auto & registry = ElementRegistry::instance();

    // Verify all steps are element-level
    for (size_t i = 0; i < pipeline.size(); ++i) {
        auto const & step = pipeline.getStep(i);
        auto const * meta = registry.getMetadata(step.transform_name);
        if (!meta) {
            throw std::runtime_error("Transform not found: " + step.transform_name);
        }
        if (meta->is_time_grouped) {
            throw std::runtime_error(
                "bindToView does not support time-grouped transforms. "
                "Step '" + step.transform_name + "' is time-grouped.");
        }
    }

    // Build chain of type-erased transform functions
    std::vector<std::function<ElementVariant(ElementVariant)>> chain;
    chain.reserve(pipeline.size());

    for (size_t i = 0; i < pipeline.size(); ++i) {
        auto const & step = pipeline.getStep(i);
        auto const * meta = registry.getMetadata(step.transform_name);

        // Capture step info and create transform function
        chain.push_back([&registry, name = step.transform_name, &step, meta](
                                ElementVariant input) -> ElementVariant {
            return registry.executeWithDynamicParams(
                    name, input, step.params,
                    meta->input_type, meta->output_type, meta->params_type);
        });
    }

    // Return composed function that applies all transforms
    return [chain = std::move(chain)](InElement const & input) -> OutElement {
        ElementVariant current{input};
        for (auto const & fn : chain) {
            current = fn(std::move(current));
        }
        return std::get<OutElement>(std::move(current));
    };
}

/**
 * @brief Inject context into pipeline step parameters
 *
 * Uses the ContextInjectorRegistry to inject context into any
 * registered context-aware parameter types.
 */
inline void injectContextIntoParams(std::any & params, TrialContext const & ctx) {
    ContextInjectorRegistry::instance().tryInject(params, ctx);
}

}  // namespace detail

template<typename InElement, typename OutElement>
ViewAdaptorFn<InElement, OutElement> bindToView(TransformPipeline const & pipeline) {
    // Build the composed function once
    auto transform_fn = detail::buildComposedTransformFn<InElement, OutElement>(pipeline);

    // Return adaptor that applies transform to each element
    return [transform_fn = std::move(transform_fn)](std::span<InElement const> input)
               -> std::vector<OutElement> {
        std::vector<OutElement> result;
        result.reserve(input.size());
        for (auto const & elem : input) {
            result.push_back(transform_fn(elem));
        }
        return result;
    };
}

template<typename InElement, typename OutElement>
ViewAdaptorFactory<InElement, OutElement> bindToViewWithContext(TransformPipeline const & pipeline) {
    // Capture a copy of the pipeline that we can modify per-context
    return [pipeline](TrialContext const & ctx) -> ViewAdaptorFn<InElement, OutElement> {
        // Create a mutable copy of the pipeline for context injection
        TransformPipeline pipeline_copy = pipeline;

        // Get non-const access to steps (we need to modify them temporarily)
        // This is safe because we're working with a copy
        auto & registry = ElementRegistry::instance();

        // Build chain with context-injected parameters
        std::vector<std::function<ElementVariant(ElementVariant)>> chain;

        for (size_t i = 0; i < pipeline_copy.size(); ++i) {
            // Get the step and inject context
            auto const & step = pipeline_copy.getStep(i);
            auto const * meta = registry.getMetadata(step.transform_name);
            if (!meta) {
                throw std::runtime_error("Transform not found: " + step.transform_name);
            }

            // Create a copy of step.params for this context
            std::any params_copy = step.params;

            // Inject context using the registry
            detail::injectContextIntoParams(params_copy, ctx);

            // Capture the params_copy by value
            chain.push_back([&registry, name = step.transform_name, params = std::move(params_copy), meta](
                                    ElementVariant input) -> ElementVariant {
                return registry.executeWithDynamicParams(
                        name, input, params,
                        meta->input_type, meta->output_type, meta->params_type);
            });
        }

        // Build and return the adaptor with context-injected transforms
        auto transform_fn = [chain = std::move(chain)](InElement const & input) -> OutElement {
            ElementVariant current{input};
            for (auto const & fn : chain) {
                current = fn(std::move(current));
            }
            return std::get<OutElement>(std::move(current));
        };

        return [transform_fn = std::move(transform_fn)](std::span<InElement const> input)
                   -> std::vector<OutElement> {
            std::vector<OutElement> result;
            result.reserve(input.size());
            for (auto const & elem : input) {
                result.push_back(transform_fn(elem));
            }
            return result;
        };
    };
}

template<typename InElement, typename Scalar>
ReducerFn<InElement, Scalar> bindReducer(TransformPipeline const & pipeline) {
    if (!pipeline.hasRangeReduction()) {
        throw std::runtime_error("Pipeline has no range reduction. Call setRangeReduction() first.");
    }

    auto const & reduction_step = pipeline.getRangeReduction().value();
    auto & reduction_registry = RangeReductionRegistry::instance();

    // Get intermediate element type from the last step's output
    auto & elem_registry = ElementRegistry::instance();
    std::type_index intermediate_type = typeid(InElement);

    if (!pipeline.empty()) {
        auto const & last_step = pipeline.getStep(pipeline.size() - 1);
        auto const * meta = elem_registry.getMetadata(last_step.transform_name);
        if (meta) {
            intermediate_type = meta->output_type;
        }
    }

    // If pipeline is empty, elements go directly to reduction
    if (pipeline.empty()) {
        // Direct reduction without transformation
        return [&reduction_registry,
                reduction_name = reduction_step.reduction_name,
                params = reduction_step.params](std::span<InElement const> input) -> Scalar {
            std::any input_any{input};
            std::any result = reduction_registry.executeErased(
                    reduction_name, typeid(InElement), input_any, params);
            return std::any_cast<Scalar>(result);
        };
    }

    // Build transform function
    using IntermediateElement = typename std::conditional_t<
        std::is_same_v<decltype(intermediate_type), std::type_index>,
        void,  // Placeholder - actual type handled via variant
        void>;

    // For now, we handle specific intermediate types
    // This could be extended with more type dispatch
    auto transform_fn = detail::buildComposedTransformFn<InElement, InElement>(pipeline);

    return [transform_fn = std::move(transform_fn),
            &reduction_registry,
            reduction_name = reduction_step.reduction_name,
            params = reduction_step.params,
            intermediate_type](std::span<InElement const> input) -> Scalar {
        // Transform all elements
        std::vector<InElement> transformed;
        transformed.reserve(input.size());
        for (auto const & elem : input) {
            // Apply the composed transform
            // Note: In full implementation, output type may differ from InElement
            transformed.push_back(transform_fn(elem));
        }

        // Apply reduction
        std::span<InElement const> transformed_span{transformed};
        std::any input_any{transformed_span};
        std::any result = reduction_registry.executeErased(
                reduction_name, intermediate_type, input_any, params);
        return std::any_cast<Scalar>(result);
    };
}

template<typename InElement, typename Scalar>
ReducerFactory<InElement, Scalar> bindReducerWithContext(TransformPipeline const & pipeline) {
    if (!pipeline.hasRangeReduction()) {
        throw std::runtime_error("Pipeline has no range reduction. Call setRangeReduction() first.");
    }

    auto const & reduction_step = pipeline.getRangeReduction().value();

    return [pipeline, reduction_step](TrialContext const & ctx) -> ReducerFn<InElement, Scalar> {
        auto & elem_registry = ElementRegistry::instance();
        auto & reduction_registry = RangeReductionRegistry::instance();

        // Get intermediate type from last step
        std::type_index intermediate_type = typeid(InElement);
        if (!pipeline.empty()) {
            auto const & last_step = pipeline.getStep(pipeline.size() - 1);
            auto const * meta = elem_registry.getMetadata(last_step.transform_name);
            if (meta) {
                intermediate_type = meta->output_type;
            }
        }

        // Build chain with context-injected parameters
        std::vector<std::function<ElementVariant(ElementVariant)>> chain;

        for (size_t i = 0; i < pipeline.size(); ++i) {
            auto const & step = pipeline.getStep(i);
            auto const * meta = elem_registry.getMetadata(step.transform_name);
            if (!meta) {
                throw std::runtime_error("Transform not found: " + step.transform_name);
            }

            // Copy params and inject context using registry
            std::any params_copy = step.params;
            detail::injectContextIntoParams(params_copy, ctx);

            chain.push_back([&elem_registry, name = step.transform_name,
                             params = std::move(params_copy), meta](
                                    ElementVariant input) -> ElementVariant {
                return elem_registry.executeWithDynamicParams(
                        name, input, params,
                        meta->input_type, meta->output_type, meta->params_type);
            });
        }

        // Build reduction params (context might affect these too)
        std::any reduction_params = reduction_step.params;

        // Return reducer that transforms and reduces
        return [chain = std::move(chain),
                &reduction_registry,
                reduction_name = reduction_step.reduction_name,
                reduction_params = std::move(reduction_params),
                intermediate_type](std::span<InElement const> input) -> Scalar {
            // Transform elements using chain
            std::vector<std::any> transformed_any;
            transformed_any.reserve(input.size());

            for (auto const & elem : input) {
                ElementVariant current{elem};
                for (auto const & fn : chain) {
                    current = fn(std::move(current));
                }
                // Store the variant result
                transformed_any.push_back(std::move(current));
            }

            // Build span of intermediate elements for reduction
            // This requires extracting from variants
            std::vector<ElementVariant> transformed;
            transformed.reserve(transformed_any.size());
            for (auto & any_elem : transformed_any) {
                transformed.push_back(std::any_cast<ElementVariant>(std::move(any_elem)));
            }

            // Apply reduction - we need to extract actual elements from variants
            // For now, we use erased execution with the intermediate type
            std::any result = reduction_registry.executeErased(
                    reduction_name, intermediate_type,
                    std::any{std::span<ElementVariant const>{transformed}},
                    reduction_params);

            return std::any_cast<Scalar>(result);
        };
    };
}

// ============================================================================
// Template Implementations: Value Projection Binding
// ============================================================================

template<typename InElement, typename Value>
ValueProjectionFn<InElement, Value> bindValueProjection(TransformPipeline const & pipeline) {
    if (pipeline.empty()) {
        throw std::runtime_error("Pipeline has no steps");
    }

    auto & registry = ElementRegistry::instance();

    // Verify all steps are element-level and collect metadata
    for (size_t i = 0; i < pipeline.size(); ++i) {
        auto const & step = pipeline.getStep(i);
        auto const * meta = registry.getMetadata(step.transform_name);
        if (!meta) {
            throw std::runtime_error("Transform not found: " + step.transform_name);
        }
        if (meta->is_time_grouped) {
            throw std::runtime_error(
                "bindValueProjection does not support time-grouped transforms. "
                "Step '" + step.transform_name + "' is time-grouped.");
        }
    }

    // Build chain of type-erased transform functions
    // Each chain element captures everything by value to avoid dangling references
    std::vector<std::function<ElementVariant(ElementVariant const &)>> chain;
    chain.reserve(pipeline.size());

    for (size_t i = 0; i < pipeline.size(); ++i) {
        auto const & step = pipeline.getStep(i);
        auto const * meta = registry.getMetadata(step.transform_name);

        // Capture all needed values by value for the closure
        chain.push_back([&registry, name = step.transform_name,
                         params = step.params,  // Copy params
                         in_type = meta->input_type,
                         out_type = meta->output_type,
                         param_type = meta->params_type](
                                ElementVariant const & input) -> ElementVariant {
            return registry.executeWithDynamicParams(
                    name, input, params, in_type, out_type, param_type);
        });
    }

    // Return projection function that applies all transforms and extracts Value
    return [chain = std::move(chain)](InElement const & input) -> Value {
        ElementVariant current{input};
        for (auto const & fn : chain) {
            current = fn(current);
        }
        return std::get<Value>(current);
    };
}

template<typename InElement, typename Value>
ValueProjectionFactory<InElement, Value> bindValueProjectionWithContext(TransformPipeline const & pipeline) {
    if (pipeline.empty()) {
        throw std::runtime_error("Pipeline has no steps");
    }

    // Capture a copy of the pipeline that we can modify per-context
    return [pipeline](TrialContext const & ctx) -> ValueProjectionFn<InElement, Value> {
        auto & registry = ElementRegistry::instance();

        // Verify all steps are element-level
        for (size_t i = 0; i < pipeline.size(); ++i) {
            auto const & step = pipeline.getStep(i);
            auto const * meta = registry.getMetadata(step.transform_name);
            if (!meta) {
                throw std::runtime_error("Transform not found: " + step.transform_name);
            }
            if (meta->is_time_grouped) {
                throw std::runtime_error(
                    "bindValueProjectionWithContext does not support time-grouped transforms. "
                    "Step '" + step.transform_name + "' is time-grouped.");
            }
        }

        // Build chain with context-injected parameters
        std::vector<std::function<ElementVariant(ElementVariant const &)>> chain;
        chain.reserve(pipeline.size());

        for (size_t i = 0; i < pipeline.size(); ++i) {
            auto const & step = pipeline.getStep(i);
            auto const * meta = registry.getMetadata(step.transform_name);

            // Create a copy of step.params for this context
            std::any params_copy = step.params;

            // Inject context using the registry
            detail::injectContextIntoParams(params_copy, ctx);

            // Capture all values by value for the closure
            chain.push_back([&registry, name = step.transform_name,
                             params = std::move(params_copy),
                             in_type = meta->input_type,
                             out_type = meta->output_type,
                             param_type = meta->params_type](
                                    ElementVariant const & input) -> ElementVariant {
                return registry.executeWithDynamicParams(
                        name, input, params, in_type, out_type, param_type);
            });
        }

        // Return projection function that applies all transforms and extracts Value
        return [chain = std::move(chain)](InElement const & input) -> Value {
            ElementVariant current{input};
            for (auto const & fn : chain) {
                current = fn(current);
            }
            return std::get<Value>(current);
        };
    };
}

}// namespace WhiskerToolbox::Transforms::V2

#endif// WHISKERTOOLBOX_V2_TRANSFORM_PIPELINE_HPP
