#ifndef GROUPING_PARAMETER_FACTORY_HPP
#define GROUPING_PARAMETER_FACTORY_HPP

#include "ParameterFactory.hpp"
#include "grouping_transforms.hpp"

class EntityGroupManager;

/**
 * @brief Extensions to ParameterFactory for handling grouping transform parameters
 * 
 * This class provides special registration methods for parameters that require
 * access to the EntityGroupManager, which can't be handled by the regular
 * parameter factory methods.
 */
class GroupingParameterFactory {
public:
    /**
     * @brief Register parameter setters for a grouping transform operation
     * 
     * @tparam ParamClass The grouping parameter class (must inherit from GroupingTransformParametersBase)
     * @param transform_name Name of the transform operation
     * @param parameter_factory Reference to the main parameter factory
     */
    template<typename ParamClass>
    static void registerGroupingTransform(std::string const& transform_name, 
                                         ParameterFactory& parameter_factory);

private:
    /**
     * @brief Create a custom parameter creator that injects EntityGroupManager
     * 
     * @tparam ParamClass The grouping parameter class
     * @param transform_name Name of the transform
     * @return ParameterSetter function that creates parameters with group manager
     */
    template<typename ParamClass>
    static ParameterSetter createGroupingParameterSetter(std::string const& transform_name);
};

// Template implementation
template<typename ParamClass>
void GroupingParameterFactory::registerGroupingTransform(std::string const& transform_name, 
                                                        ParameterFactory& parameter_factory) {
    static_assert(std::is_base_of_v<GroupingTransformParametersBase, ParamClass>, 
                  "ParamClass must inherit from GroupingTransformParametersBase");

    // Register a special setter that creates the parameter object with EntityGroupManager
    parameter_factory.registerParameterSetter(transform_name, "_create_grouping_params", 
        createGroupingParameterSetter<ParamClass>(transform_name));
}

template<typename ParamClass>
ParameterSetter GroupingParameterFactory::createGroupingParameterSetter(std::string const& transform_name) {
    return [transform_name](TransformParametersBase* param_obj, nlohmann::json const& json_value, DataManager* dm) -> bool {
        // For grouping parameters, the EntityGroupManager is now set automatically
        // by the TransformPipeline::createParametersFromJson method, so we don't need
        // to do anything special here - just return true to indicate success
        return true;
    };
}

#endif//GROUPING_PARAMETER_FACTORY_HPP