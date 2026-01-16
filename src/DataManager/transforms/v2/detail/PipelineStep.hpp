#ifndef PIPELINE_STEP_HPP
#define PIPELINE_STEP_HPP

#include "transforms/v2/core/ElementRegistry.hpp"
#include "transforms/v2/extension/TransformTypes.hpp"

#include <any>
#include <functional>
#include <string>

namespace WhiskerToolbox::Transforms::V2 {


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

} // namespace WhiskerToolbox::Transforms::V2

#endif // PIPELINE_STEP_HPP