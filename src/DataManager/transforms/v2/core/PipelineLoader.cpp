
#include "PipelineLoader.hpp"

namespace WhiskerToolbox::Transforms::V2::Examples {

PipelineStep createPipelineStepFromRegistry(
        ElementRegistry const & registry,
        std::string const & transform_name,
        std::any const & params_any) {
    auto const * meta = registry.getMetadata(transform_name);
    if (!meta) {
        throw std::runtime_error("Transform '" + transform_name + "' not found");
    }

    auto & factory_registry = getPipelineStepFactoryRegistry();
    auto it = factory_registry.find(meta->params_type);

    if (it == factory_registry.end()) {
        throw std::runtime_error(
                "No PipelineStep factory registered for parameter type: " +
                std::string(meta->params_type.name()));
    }

    return it->second(transform_name, params_any);
}

rfl::Result<PipelineStep> loadStepFromDescriptor(PipelineStepDescriptor const & descriptor) {
    auto & registry = ElementRegistry::instance();

    // Validate transform exists
    auto const * metadata = registry.getMetadata(descriptor.transform_name);
    if (!metadata) {
        return rfl::Error("Transform '" + descriptor.transform_name + "' not found in registry");
    }

    // Check if step is disabled
    if (descriptor.enabled.has_value() && !descriptor.enabled.value()) {
        return rfl::Error("Step '" + descriptor.step_id + "' is disabled");
    }

    // Load parameters if provided
    if (descriptor.parameters.has_value()) {
        // Convert rfl::Generic back to JSON string for registry-based loading
        auto json_str = rfl::json::write(descriptor.parameters.value());

        // Use registry to deserialize parameters based on transform metadata
        auto params_any = loadParametersForTransform(descriptor.transform_name, json_str);
        if (!params_any.has_value()) {
            return rfl::Error("Failed to load parameters for transform '" +
                              descriptor.transform_name + "' in step '" +
                              descriptor.step_id + "'. Check that parameters match the expected type and validation rules.");
        }

        // Create PipelineStep using factory registry (no manual dispatch!)
        try {
            auto step = createPipelineStepFromRegistry(registry, descriptor.transform_name, params_any);
            return rfl::Result<PipelineStep>(std::move(step));
        } catch (std::exception const & e) {
            return rfl::Error("Failed to create pipeline step for transform '" +
                              descriptor.transform_name + "': " + e.what());
        }
    } else {
        // No parameters - create step with NoParams
        return rfl::Result<PipelineStep>(PipelineStep(descriptor.transform_name));
    }
}

rfl::Result<TransformPipeline> loadPipelineFromJson(std::string const & json_str) {
    // Parse JSON into descriptor
    auto descriptor_result = rfl::json::read<PipelineDescriptor>(json_str);
    if (!descriptor_result) {
        return rfl::Error("Failed to parse pipeline JSON: " +
                          std::string(descriptor_result.error()->what()));
    }

    auto const & descriptor = descriptor_result.value();

    // Validate we have at least one step
    if (descriptor.steps.empty()) {
        return rfl::Error("Pipeline must have at least one step");
    }

    // Create empty pipeline
    TransformPipeline pipeline;

    // Load each step
    for (size_t i = 0; i < descriptor.steps.size(); ++i) {
        auto const & step_desc = descriptor.steps[i];

        auto step_result = loadStepFromDescriptor(step_desc);
        if (!step_result) {
            return rfl::Error("Failed to load step " + std::to_string(i) +
                              " ('" + step_desc.step_id + "'): " +
                              std::string(step_result.error()->what()));
        }

        // Add step to pipeline
        pipeline.addStep(step_result.value());
    }

    return rfl::Result<TransformPipeline>(std::move(pipeline));
}

rfl::Result<TransformPipeline> loadPipelineFromFile(std::string const & filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return rfl::Error("Failed to open file: " + filepath);
        }

        std::string json_str((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());

        return loadPipelineFromJson(json_str);
    } catch (std::exception const & e) {
        return rfl::Error("Exception reading file: " + std::string(e.what()));
    }
}

std::string savePipelineToJson(PipelineDescriptor const & descriptor) {
    return rfl::json::write(descriptor);
}


}// namespace WhiskerToolbox::Transforms::V2::Examples