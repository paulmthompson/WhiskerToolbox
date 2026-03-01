#include "ParameterIO.hpp"

#include "core/ElementRegistry.hpp"

namespace WhiskerToolbox::Transforms::V2::Examples {

std::any loadParametersForTransform(
        std::string const & transform_name,
        std::string const & json_str) {
    auto & registry = ElementRegistry::instance();
    return registry.deserializeParameters(transform_name, json_str);
}

}// namespace WhiskerToolbox::Transforms::V2::Examples