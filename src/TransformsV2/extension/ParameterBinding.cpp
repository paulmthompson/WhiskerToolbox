
#include "ParameterBinding.hpp"

namespace WhiskerToolbox::Transforms::V2 {

std::unordered_map<std::type_index, BindingApplicatorFn> &
getBindingApplicatorRegistry() {
    static std::unordered_map<std::type_index, BindingApplicatorFn> registry;
    return registry;
}

std::any applyBindingsErased(
        std::type_index params_type,
        std::any const & base_params,
        std::map<std::string, std::string> const & bindings,
        PipelineValueStore const & store) {
    if (bindings.empty()) {
        return base_params;
    }

    auto & registry = getBindingApplicatorRegistry();
    auto it = registry.find(params_type);

    if (it == registry.end()) {
        throw std::runtime_error(
                "No binding applicator registered for parameter type: " +
                std::string(params_type.name()));
    }

    return it->second(base_params, bindings, store);
}

std::any tryApplyBindingsErased(
        std::type_index params_type,
        std::any const & base_params,
        std::map<std::string, std::string> const & bindings,
        PipelineValueStore const & store) noexcept {
    try {
        return applyBindingsErased(params_type, base_params, bindings, store);
    } catch (...) {
        return base_params;
    }
}

bool hasBindingApplicator(std::type_index params_type) {
    auto & registry = getBindingApplicatorRegistry();
    return registry.find(params_type) != registry.end();
}

}// namespace WhiskerToolbox::Transforms::V2