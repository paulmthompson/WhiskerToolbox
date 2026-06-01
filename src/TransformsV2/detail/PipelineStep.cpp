#include "PipelineStep.hpp"

#include "extension/ParameterBinding.hpp"  // tryApplyBindingsErased

namespace WhiskerToolbox::Transforms::V2 {

void PipelineStep::applyBindings(PipelineValueStore const & store) const {
    if (param_bindings.empty()) {
        return;
    }

    // Use type-erased binding application from registry
    params = tryApplyBindingsErased(
            params.type(),
            params,
            param_bindings,
            store);
}

bool PipelineStep::hasBindings() const noexcept {
    return !param_bindings.empty();
}

}// namespace WhiskerToolbox::Transforms::V2