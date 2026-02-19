/**
 * @file TypeChainResolver.cpp
 * @brief Implementation of data-free type-chain resolution
 */

#include "TypeChainResolver.hpp"

#include "ElementRegistry.hpp"
#include "detail/ContainerTraits.hpp"

namespace WhiskerToolbox::Transforms::V2 {

// ---------------------------------------------------------------------------
// Local helper — mirrors the dispatch in TransformPipeline::execute()
// ---------------------------------------------------------------------------

namespace {

/**
 * @brief Resolve the container type from an element type and ragged state
 *
 * For element types other than float the mapping is unambiguous.
 * For float, it depends on whether the chain is currently in a
 * ragged context (e.g., RaggedAnalogTimeSeries) or not.
 */
std::type_index resolveContainerFromElement(std::type_index element_type,
                                            bool is_ragged) {
    if (element_type == typeid(float)) {
        return is_ragged
                       ? TypeIndexMapper::stringToContainer("RaggedAnalogTimeSeries")
                       : TypeIndexMapper::stringToContainer("AnalogTimeSeries");
    }
    return TypeIndexMapper::elementToContainer(element_type);
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// resolveTypeChain
// ---------------------------------------------------------------------------

TypeChainResult resolveTypeChain(
        std::type_index input_container_type,
        std::span<std::string const> step_names) {

    TypeChainResult result;
    auto & registry = ElementRegistry::instance();

    auto current_container = input_container_type;
    std::type_index current_element{typeid(void)};
    bool is_ragged = false;

    // --- Derive initial element type from the container ----------------------
    try {
        current_element = TypeIndexMapper::containerToElement(current_container);
    } catch (...) {
        // Unknown container — can't derive element type.
        result.output_element_type = typeid(void);
        result.output_container_type = input_container_type;
        result.all_valid = step_names.empty();
        for (auto const & name : step_names) {
            (void) name;
            result.steps.push_back({"Unknown", "Unknown", false});
        }
        return result;
    }

    // --- Derive initial ragged state -----------------------------------------
    is_ragged = TypeIndexMapper::isContainerRagged(current_container);

    // --- Walk the chain ------------------------------------------------------
    for (auto const & name : step_names) {
        StepTypeInfo info;
        info.input_type_name = TypeIndexMapper::containerToString(current_container);

        // ---- Container-level transform? ------------------------------------
        auto const * cmeta = registry.getContainerMetadata(name);
        if (cmeta) {
            info.is_valid = (cmeta->input_container_type == current_container);

            auto const output_container = cmeta->output_container_type;
            info.output_type_name = TypeIndexMapper::containerToString(output_container);

            if (info.is_valid) {
                current_container = output_container;
                try {
                    current_element = TypeIndexMapper::containerToElement(current_container);
                } catch (...) { /* keep previous element type */ }
                is_ragged = TypeIndexMapper::isContainerRagged(current_container);
            }

        } else {
            // ---- Element-level transform ------------------------------------
            auto const * meta = registry.getMetadata(name);
            if (!meta) {
                info.is_valid = false;
                info.output_type_name = "Unknown";
                result.steps.push_back(info);
                result.all_valid = false;
                continue;
            }

            info.is_valid = (meta->input_type == current_element);

            if (info.is_valid) {
                current_element = meta->output_type;

                // Mirror TransformPipeline::execute() ragged tracking
                if (meta->is_time_grouped) {
                    is_ragged = !meta->produces_single_output;
                }
                // Element-wise transforms preserve ragged state (no change)

                // Resolve container from element + ragged state
                try {
                    current_container = resolveContainerFromElement(current_element, is_ragged);
                } catch (...) { /* keep previous container */ }
            }

            // Output name: when valid, use the resolved container.
            // When invalid, show what the transform would produce in isolation.
            try {
                if (info.is_valid) {
                    info.output_type_name = TypeIndexMapper::containerToString(current_container);
                } else {
                    auto hypothetical = resolveContainerFromElement(meta->output_type, is_ragged);
                    info.output_type_name = TypeIndexMapper::containerToString(hypothetical);
                }
            } catch (...) {
                info.output_type_name = "Unknown";
            }
        }

        if (!info.is_valid) {
            result.all_valid = false;
        }
        result.steps.push_back(info);
    }

    result.output_element_type = current_element;
    result.output_container_type = current_container;
    return result;
}

} // namespace WhiskerToolbox::Transforms::V2
