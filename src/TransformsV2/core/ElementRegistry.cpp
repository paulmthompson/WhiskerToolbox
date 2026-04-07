#include "ElementRegistry.hpp"

namespace WhiskerToolbox::Transforms::V2 {

// ========================================================================
// Container Transform Registration and Execution
// ========================================================================

bool ElementRegistry::isContainerTransform(std::string const & name) const {
    return container_metadata_.find(name) != container_metadata_.end();
}

ContainerTransformMetadata const * ElementRegistry::getContainerMetadata(std::string const & name) const {
    auto it = container_metadata_.find(name);
    return (it != container_metadata_.end()) ? &it->second : nullptr;
}

std::vector<std::string> ElementRegistry::getContainerTransformsForInputType(std::type_index input_type) const {
    auto it = container_input_to_names_.find(input_type);
    if (it != container_input_to_names_.end()) {
        return it->second;
    }
    return {};
}

std::shared_ptr<IContainerExecutor> ElementRegistry::createContainerExecutor(
        std::string const & name,
        std::any const & params_any) const {

    auto const * meta = getContainerMetadata(name);
    if (!meta) {
        return nullptr;
    }

    TypeTriple key{
            meta->input_container_type,
            meta->output_container_type,
            meta->params_type};

    auto factory_it = container_executor_factories_.find(key);
    if (factory_it == container_executor_factories_.end()) {
        return nullptr;
    }

    return factory_it->second(params_any);
}

DataTypeVariant ElementRegistry::executeContainerTransformDynamic(
        std::string const & name,
        DataTypeVariant const & input_variant,
        std::any const & params_any,
        ComputeContext const & ctx) const {

    auto executor = createContainerExecutor(name, params_any);
    if (!executor) {
        throw std::runtime_error("Container transform not found or failed to create executor: " + name);
    }

    return executor->execute(name, input_variant, ctx);
}

// ========================================================================
// Query Interface
// ========================================================================

std::vector<std::string> ElementRegistry::getTransformsForInputType(std::type_index input_type) const {
    auto it = input_type_to_names_.find(input_type);
    if (it != input_type_to_names_.end()) {
        return it->second;
    }
    return {};
}

std::vector<std::string> ElementRegistry::getTransformsForOutputType(std::type_index output_type) const {
    auto it = output_type_to_names_.find(output_type);
    if (it != output_type_to_names_.end()) {
        return it->second;
    }
    return {};
}

TransformMetadata const * ElementRegistry::getMetadata(std::string const & name) const {
    auto it = metadata_.find(name);
    return (it != metadata_.end()) ? &it->second : nullptr;
}

// ========================================================================
// Parameter Schema Query
// ========================================================================


ParameterSchema const * ElementRegistry::getParameterSchema(std::string const & transform_name) const {
    // Find the params type_index from either element or container metadata
    std::type_index params_type = typeid(void);

    auto const * element_meta = getMetadata(transform_name);
    if (element_meta) {
        params_type = element_meta->params_type;
    } else {
        auto const * container_meta = getContainerMetadata(transform_name);
        if (container_meta) {
            params_type = container_meta->params_type;
        } else {
            return nullptr;
        }
    }

    auto it = schema_cache_.find(params_type);
    if (it != schema_cache_.end()) {
        return &it->second;
    }

    // Generate and cache the schema on first access
    auto factory_it = schema_factories_.find(params_type);
    if (factory_it == schema_factories_.end()) {
        return nullptr;
    }

    auto [cache_it, _] = schema_cache_.emplace(params_type, factory_it->second());
    return &cache_it->second;
}

ParameterSchema const * ElementRegistry::getParameterSchemaByType(std::type_index params_type) const {
    auto it = schema_cache_.find(params_type);
    if (it != schema_cache_.end()) {
        return &it->second;
    }

    auto factory_it = schema_factories_.find(params_type);
    if (factory_it == schema_factories_.end()) {
        return nullptr;
    }

    auto [cache_it, _] = schema_cache_.emplace(params_type, factory_it->second());
    return &cache_it->second;
}

bool ElementRegistry::hasTransform(std::string const & name) const {
    return metadata_.find(name) != metadata_.end() ||
           container_metadata_.find(name) != container_metadata_.end();
}

bool ElementRegistry::hasElementTransform(std::string const & name) const {
    return metadata_.find(name) != metadata_.end();
}

std::vector<std::string> ElementRegistry::getAllTransformNames() const {
    std::vector<std::string> names;
    names.reserve(metadata_.size());
    for (auto const & [name, _]: metadata_) {
        names.push_back(name);
    }
    return names;
}

std::any ElementRegistry::deserializeParameters(
        std::string const & transform_name,
        std::string const & json_str) const {
    // Get metadata to find parameter type - check both element and container transforms
    std::type_index params_type = typeid(void);

    auto const * element_metadata = getMetadata(transform_name);
    if (element_metadata) {
        params_type = element_metadata->params_type;
    } else {
        auto const * container_metadata = getContainerMetadata(transform_name);
        if (container_metadata) {
            params_type = container_metadata->params_type;
        } else {
            return std::any{};// Transform not found in either registry
        }
    }

    // Look up deserializer for this parameter type
    auto it = param_deserializers_.find(params_type);
    if (it == param_deserializers_.end()) {
        return std::any{};// No deserializer registered
    }

    // Deserialize
    try {
        return it->second(json_str);
    } catch (...) {
        return std::any{};// Deserialization failed
    }
}

bool ElementRegistry::validateParameters(
        std::string const & transform_name,
        std::any const & params_any) const {
    // Get metadata to find parameter type - check both element and container transforms
    std::type_index params_type = typeid(void);

    auto const * element_metadata = getMetadata(transform_name);
    if (element_metadata) {
        params_type = element_metadata->params_type;
    } else {
        auto const * container_metadata = getContainerMetadata(transform_name);
        if (container_metadata) {
            params_type = container_metadata->params_type;
        } else {
            return false;// Transform not found in either registry
        }
    }

    // Look up validator for this parameter type
    auto it = param_validators_.find(params_type);
    if (it == param_validators_.end()) {
        return false;// No validator registered
    }

    // Validate
    return it->second(params_any);
}

}// namespace WhiskerToolbox::Transforms::V2