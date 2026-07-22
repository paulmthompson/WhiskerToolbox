/**
 * @file PostEncoderModuleRegistry.hpp
 * @brief Singleton registry of available post-encoder modules.
 */

#ifndef NEURALYZER_POST_ENCODER_MODULE_REGISTRY_HPP
#define NEURALYZER_POST_ENCODER_MODULE_REGISTRY_HPP

#include "PostEncoderModule.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "ParameterSchema/ParameterSchema.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace dl {

/**
 * @brief Display and constraint metadata for a registered post-encoder module.
 */
struct PostEncoderModuleMetadata {
    std::string key;
    std::string display_name;
    std::string description;
    /** When true, module maps @c [B,C,H,W] to @c [B,C]. */
    bool collapses_spatial_dims = false;
};

/**
 * @brief A registered post-encoder module with schema and factory.
 */
struct PostEncoderModuleEntry {
    PostEncoderModuleMetadata metadata;
    ParameterSchema schema;
    std::function<std::unique_ptr<PostEncoderModule>(
            std::string const & params_json,
            ImageSize source_image_size)>
            factory;
};

/**
 * @brief Singleton registry mapping module keys to post-encoder descriptors.
 *
 * Modules self-register at static-init time via `RegisterPostEncoderModule`.
 * The UI queries `moduleKeys()` and `getSchema()`; `SlotAssembler` calls
 * `create()` to instantiate modules from persisted JSON parameters.
 */
class PostEncoderModuleRegistry {
public:
    /**
     * @brief Get the global singleton instance.
     */
    static PostEncoderModuleRegistry & instance();

    /**
     * @brief Register a post-encoder module.
     *
     * @pre entry.metadata.key must not be empty.
     * @pre entry.factory must not be null.
     */
    void registerModule(PostEncoderModuleEntry entry);

    /**
     * @brief Sorted list of all registered module keys.
     */
    [[nodiscard]] std::vector<std::string> moduleKeys() const;

    /**
     * @brief Whether a module key is registered.
     */
    [[nodiscard]] bool hasModule(std::string const & key) const;

    /**
     * @brief Query metadata for a registered module.
     */
    [[nodiscard]] std::optional<PostEncoderModuleMetadata>
    getMetadata(std::string const & key) const;

    /**
     * @brief Query the parameter schema for a registered module.
     */
    [[nodiscard]] std::optional<ParameterSchema>
    getSchema(std::string const & key) const;

    /**
     * @brief Instantiate a module by key and JSON parameters.
     *
     * @return nullptr for @c "none", empty key, or unknown keys.
     */
    [[nodiscard]] std::unique_ptr<PostEncoderModule>
    create(std::string const & key,
           std::string const & params_json,
           ImageSize source_image_size = {}) const;

    /**
     * @brief Whether the module collapses spatial dimensions.
     *
     * @return false for @c "none", empty key, or unknown keys.
     */
    [[nodiscard]] bool collapsesSpatialDims(std::string const & key) const;

    /**
     * @brief Propagate output shape through a module without running inference.
     *
     * @return std::nullopt if the key is not registered or module creation fails.
     */
    [[nodiscard]] std::optional<std::vector<int64_t>>
    outputShape(std::string const & key,
                std::vector<int64_t> const & input_shape,
                std::string const & params_json,
                ImageSize source_image_size = {}) const;

private:
    PostEncoderModuleRegistry() = default;

    std::unordered_map<std::string, PostEncoderModuleEntry> _entries;
};

}// namespace dl

#endif// NEURALYZER_POST_ENCODER_MODULE_REGISTRY_HPP
