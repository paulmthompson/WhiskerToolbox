#ifndef NEURALYZER_V2_RANGE_REDUCTION_STEP_HPP
#define NEURALYZER_V2_RANGE_REDUCTION_STEP_HPP

#include <any>
#include <string>
#include <typeindex>

namespace WhiskerToolbox::Transforms::V2 {

/**
 * @brief Descriptor for a terminal range reduction in a pipeline
 *
 * This is stored in TransformPipeline when setRangeReduction() is called.
 * It contains the reduction name and type-erased parameters.
 */
struct RangeReductionStep {
    /// Name of the registered range reduction
    std::string reduction_name;

    /// Type-erased parameters for the reduction
    std::any params;

    /// Input element type (for validation)
    std::type_index input_type = typeid(void);

    /// Output scalar type
    std::type_index output_type = typeid(void);

    /// Parameter type
    std::type_index params_type = typeid(void);

    RangeReductionStep() = default;

    template<typename Params>
    RangeReductionStep(std::string name, Params p)
        : reduction_name(std::move(name)),
          params(std::move(p)),
          params_type(typeid(Params)) {}
};

}// namespace WhiskerToolbox::Transforms::V2

#endif // NEURALYZER_V2_RANGE_REDUCTION_STEP_HPP