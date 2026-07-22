/**
 * @file RegisterPostEncoderModule.hpp
 * @brief RAII helper for static registration of post-encoder modules.
 */

#ifndef NEURALYZER_REGISTER_POST_ENCODER_MODULE_HPP
#define NEURALYZER_REGISTER_POST_ENCODER_MODULE_HPP

#include "PostEncoderModuleRegistry.hpp"

#include "ParameterSchema/ParameterSchema.hpp"

#include <rfl/DefaultIfMissing.hpp>
#include <rfl/json.hpp>

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace dl {

/**
 * @brief RAII helper for compile-time post-encoder module registration.
 *
 * @tparam Params reflect-cpp parameter struct for the module.
 */
template<typename Params>
class RegisterPostEncoderModule {
public:
    /**
     * @brief Register a post-encoder module at static initialization time.
     *
     * @param key Registry key (e.g. @c "global_avg_pool").
     * @param display_name Human-readable name for UI lists.
     * @param description Short module description.
     * @param collapses_spatial_dims Whether the module maps @c [B,C,H,W] to @c [B,C].
     * @param typed_factory Factory that creates the module from typed params.
     */
    RegisterPostEncoderModule(
            std::string key,
            std::string display_name,
            std::string description,
            bool collapses_spatial_dims,
            std::function<std::unique_ptr<PostEncoderModule>(
                    Params const &,
                    ImageSize)> typed_factory) {

        auto factory =
                [typed_factory = std::move(typed_factory),
                 key = key](std::string const & params_json,
                            ImageSize source_image_size) -> std::unique_ptr<PostEncoderModule> {
                    auto parsed =
                            rfl::json::read<Params, rfl::DefaultIfMissing>(params_json);
                    if (!parsed) {
                        throw std::runtime_error(
                                "PostEncoder module '" + key +
                                "': failed to parse parameters");
                    }
                    return typed_factory(*parsed, source_image_size);
                };

        PostEncoderModuleEntry entry;
        entry.metadata.key = key;
        entry.metadata.display_name = std::move(display_name);
        entry.metadata.description = std::move(description);
        entry.metadata.collapses_spatial_dims = collapses_spatial_dims;
        entry.schema = extractParameterSchema<Params>();
        entry.factory = std::move(factory);

        PostEncoderModuleRegistry::instance().registerModule(std::move(entry));
    }
};

}// namespace dl

#endif// NEURALYZER_REGISTER_POST_ENCODER_MODULE_HPP
