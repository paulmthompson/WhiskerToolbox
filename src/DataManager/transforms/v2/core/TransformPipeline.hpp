#ifndef WHISKERTOOLBOX_V2_TRANSFORM_PIPELINE_HPP
#define WHISKERTOOLBOX_V2_TRANSFORM_PIPELINE_HPP

#include "ContainerTraits.hpp"
#include "ContainerTransform.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DataManagerTypes.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "ElementRegistry.hpp"
#include "TransformTypes.hpp"

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
    std::any params;// Type-erased parameters

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

inline void pushToBatch(BatchVariant & batch, ElementVariant const & element) {
    std::visit([&](auto & vec, auto const & elem) {
        using VecT = std::decay_t<decltype(vec)>;
        using ElemT = std::decay_t<decltype(elem)>;
        // Check if vector value type matches element type
        if constexpr (std::is_same_v<typename VecT::value_type, ElemT>) {
            vec.push_back(elem);
        } else {
            // This should not happen in a correctly typed pipeline
            throw std::runtime_error("Type mismatch in pushToBatch: Vector and Element types do not match");
        }
    },
               batch, element);
}

inline BatchVariant initBatchFromElement(ElementVariant const & element) {
    return std::visit([](auto const & elem) -> BatchVariant {
        using ElemT = std::decay_t<decltype(elem)>;
        return std::vector<ElemT>{elem};
    },
                      element);
}

inline size_t getBatchSize(BatchVariant const & batch) {
    return std::visit([](auto const & vec) { return vec.size(); }, batch);
}

inline void clearBatch(BatchVariant & batch) {
    std::visit([](auto & vec) { vec.clear(); }, batch);
}

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
                        meta->params_type
                    );
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
    DataTypeVariant execute(InputContainer const & input) const {
        if (steps_.empty()) {
            throw std::runtime_error("Pipeline has no steps");
        }

        auto & registry = ElementRegistry::instance();

        // 1. Compile pipeline into segments
        std::vector<Segment> segments;
        bool is_ragged = InputContainer::DataTraits::is_ragged;

        for (size_t i = 0; i < steps_.size(); ++i) {
            auto const & step = steps_[i];
            auto const * meta = registry.getMetadata(step.transform_name);
            if (!meta) throw std::runtime_error("Transform not found: " + step.transform_name);

            if (meta->is_time_grouped) {
                // Time-grouped transform is always its own segment
                Segment seg;
                seg.is_element_wise = false;
                seg.step_indices = {i};
                seg.input_type = meta->input_type;
                seg.output_type = meta->output_type;
                segments.push_back(std::move(seg));

                // Assume time-grouped transforms produce ragged output unless proven otherwise
                if (meta->produces_single_output) {
                    is_ragged = false;
                } else {
                    is_ragged = true;
                }
            } else {
                // Element-wise transform: merge with previous if possible
                if (!segments.empty() && segments.back().is_element_wise) {
                    segments.back().step_indices.push_back(i);
                    segments.back().output_type = meta->output_type;
                } else {
                    Segment seg;
                    seg.is_element_wise = true;
                    seg.step_indices = {i};
                    seg.input_type = meta->input_type;
                    seg.output_type = meta->output_type;
                    segments.push_back(std::move(seg));
                }
                // Element-wise preserves raggedness
            }
        }

        // 2. Build fused functions for element segments
        for (auto & seg: segments) {
            if (seg.is_element_wise) {
                std::vector<std::function<ElementVariant(ElementVariant)>> chain;
                for (size_t idx: seg.step_indices) {
                    auto const & step = steps_[idx];
                    auto const * meta = registry.getMetadata(step.transform_name);
                    chain.push_back(buildTypeErasedFunction(step, meta));
                }

                seg.fused_fn = [chain = std::move(chain)](ElementVariant input) -> ElementVariant {
                    ElementVariant current = std::move(input);
                    for (auto const & fn: chain) {
                        current = fn(std::move(current));
                    }
                    return current;
                };
            }
        }

        // 3. Determine output container type and dispatch
        std::type_index final_type = segments.back().output_type;

        if (final_type == typeid(float)) {
            if (is_ragged) {
                return executeImpl<InputContainer, RaggedAnalogTimeSeries>(input, segments);
            } else {
                return executeImpl<InputContainer, AnalogTimeSeries>(input, segments);
            }
        } else if (final_type == typeid(Mask2D)) {
            return executeImpl<InputContainer, MaskData>(input, segments);
        } else if (final_type == typeid(Line2D)) {
            return executeImpl<InputContainer, LineData>(input, segments);
        } else if (final_type == typeid(Point2D<float>)) {
            return executeImpl<InputContainer, PointData>(input, segments);
        } else {
            throw std::runtime_error("Unsupported output element type: " + std::string(final_type.name()));
        }
    }

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
            // Has time-grouped transforms - use standard execution
            return execute<InputContainer, OutputContainer>(input);
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
    auto executeFromView(View&& view) const {
        if (steps_.empty()) {
            throw std::runtime_error("Pipeline has no steps");
        }

        auto & registry = ElementRegistry::instance();

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

        auto const& head_step = steps_.front();
        auto const* head_meta = registry.getMetadata(head_step.transform_name);

        // Build Head Function: InputElement -> ElementVariant
        auto head_fn = [this, step = head_step, meta = head_meta](InputElement const& input) -> ElementVariant {
            auto& reg = ElementRegistry::instance();
            // Wrap input in std::any to pass through the generic interface
            std::any input_any{input};
            return reg.executeWithDynamicParamsAny(
                step.transform_name, 
                input_any, 
                step.params, 
                std::type_index(typeid(InputElement)), // Use actual input type
                meta->output_type, 
                meta->params_type
            );
        };

        // Build Tail Chain: ElementVariant -> ElementVariant
        std::vector<std::function<ElementVariant(ElementVariant)>> tail_chain;
        tail_chain.reserve(steps_.size() - 1);

        for (size_t i = 1; i < steps_.size(); ++i) {
            auto const& step = steps_[i];
            auto const* meta = registry.getMetadata(step.transform_name);
            tail_chain.push_back(buildTypeErasedFunction(step, meta));
        }

        // Compose Head and Tail
        auto composed_fn = [head = std::move(head_fn), tail = std::move(tail_chain)](InputElement const& input) -> ElementVariant {
            // Execute Head
            ElementVariant current = head(input);
            
            // Execute Tail
            for (auto const& transform : tail) {
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
                   auto const& get_input_ref = [](auto const& val) -> InputElement const& {
                       if constexpr (std::is_convertible_v<decltype(val), InputElement const&>) {
                           return val;
                       } else if constexpr (requires { val.data; }) {
                           // Try unwrapping .data (e.g. DataEntry<T>)
                           if constexpr (std::is_convertible_v<decltype(val.data), InputElement const&>) {
                               return val.data;
                           } else {
                               // If .data exists but isn't convertible, we can't do much.
                               // This might happen if InputElement is wrong.
                               // Return val and let compiler error on mismatch.
                               return (InputElement const&)val; 
                           }
                       } else {
                           // Fallback: force cast/conversion and let compiler error if invalid
                           return (InputElement const&)val;
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
            TransformMetadata const * meta) const {
        auto & registry = ElementRegistry::instance();

        // Dispatch based on input and output types from metadata
        // This is where we handle the type erasure/recovery

        auto input_type = meta->input_type;
        auto output_type = meta->output_type;
        auto params_type = meta->params_type;

        // Build a lambda that captures the transform and its parameters
        // The lambda knows the concrete types and can do the variant conversions
        return buildTypeErasedFunctionWithParams(step, input_type, output_type, params_type);
    }

    /**
     * @brief Build type-erased function for transforms with parameters
     * 
     * This version extracts parameters from step.params and captures them.
     */
    std::function<ElementVariant(ElementVariant)> buildTypeErasedFunctionWithParams(
            PipelineStep const & step,
            std::type_index input_type,
            std::type_index output_type,
            std::type_index params_type) const {
        auto & registry = ElementRegistry::instance();

        // Use the registry's dynamic parameter execution
        // Capture the types and parameters for later execution
        return [&registry,
                name = step.transform_name,
                params = step.params,
                input_type,
                output_type,
                params_type](ElementVariant input) -> ElementVariant {
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
    std::function<OutputElement(InputElement const &)> buildElementFunction(
            PipelineStep const & step,
            std::type_index param_type) const {
        auto & registry = ElementRegistry::instance();

        // Dispatch based on parameter type
        if (param_type == typeid(NoParams)) {
            auto const & params = std::any_cast<NoParams const &>(step.params);
            return [&registry, name = step.transform_name, params](InputElement const & input) -> OutputElement {
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
            InputContainer const & input,
            PipelineStep const & step,
            std::type_index param_type) const
            -> std::shared_ptr<RaggedContainerFor_t<OutputElement>> {
        using OutputContainer = RaggedContainerFor_t<OutputElement>;
        auto & registry = ElementRegistry::instance();

        auto output = std::make_shared<OutputContainer>();
        output->setTimeFrame(input.getTimeFrame());

        auto const * meta = registry.getMetadata(step.transform_name);
        if (!meta) {
            throw std::runtime_error("Transform not found: " + step.transform_name);
        }

        // Transform each element
        for (auto const & [time, entry]: input.elements()) {
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
     * @brief Apply time-grouped transform with dynamic executor creation
     */
    template<typename InputContainer, typename OutputContainer, typename InputElement, typename OutputElement>
    std::shared_ptr<OutputContainer> applyTimeGroupedTransformGeneric(
            InputContainer const & input,
            PipelineStep const & step,
            std::type_index param_type) const {
        auto & registry = ElementRegistry::instance();

        // For AnalogTimeSeries output, collect into map
        if constexpr (std::is_same_v<OutputContainer, AnalogTimeSeries>) {
            auto time_indices = input.getTimeIndices();
            std::map<int, OutputElement> output_map;

            for (auto time: time_indices) {
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
};


/**
 * @brief Execute pipeline on variant input
 */
inline DataTypeVariant executePipeline(DataTypeVariant const & input, TransformPipeline const & pipeline) {
    return std::visit([&](auto const & ptr) -> DataTypeVariant {
        using T = typename std::remove_reference_t<decltype(*ptr)>;
        // Check if T is a valid input container (has DataTraits)
        if constexpr (TypeTraits::HasDataTraits<T>) {
            return pipeline.execute<T>(*ptr);
        } else {
            throw std::runtime_error("Unsupported input container type in variant");
        }
    },
                      input);
}

}// namespace WhiskerToolbox::Transforms::V2

#endif// WHISKERTOOLBOX_V2_TRANSFORM_PIPELINE_HPP
