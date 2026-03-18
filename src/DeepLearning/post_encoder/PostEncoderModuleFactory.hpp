/// @file PostEncoderModuleFactory.hpp
/// @brief Factory for creating PostEncoderModule instances by string key.

#ifndef WHISKERTOOLBOX_POST_ENCODER_MODULE_FACTORY_HPP
#define WHISKERTOOLBOX_POST_ENCODER_MODULE_FACTORY_HPP

#include "PostEncoderModule.hpp"
#include "SpatialPointExtractModule.hpp"

#include "CoreGeometry/ImageSize.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace dl {

/// User-configurable params for GlobalAvgPoolModule (none).
struct GlobalAvgPoolModuleParams {};

/// User-configurable params for SpatialPointExtractModule.
struct SpatialPointModuleParams {
    InterpolationMode interpolation = InterpolationMode::Nearest;
};

/// Factory for creating `PostEncoderModule` instances by name.
///
/// Registered module keys:
///   - `"none"`          → returns nullptr (no post-processing)
///   - `"global_avg_pool"` → `GlobalAvgPoolModule`
///   - `"spatial_point"`   → `SpatialPointExtractModule`
///
/// Example:
/// @code
///     auto module = dl::PostEncoderModuleFactory::create("global_avg_pool");
///     // module is a GlobalAvgPoolModule
/// @endcode
class PostEncoderModuleFactory {
public:
    /// Create a module instance by name.
    ///
    /// @param module_name       Registered key (e.g. "global_avg_pool").
    /// @param source_image_size Source image dimensions (for spatial_point
    ///                          coordinate scaling).
    /// @param params            User-configurable params for spatial_point.
    /// @return `unique_ptr<PostEncoderModule>`, or `nullptr` for "none" or
    ///         unknown keys.
    [[nodiscard]] static std::unique_ptr<PostEncoderModule>
    create(std::string const & module_name,
           ImageSize source_image_size = {},
           SpatialPointModuleParams const & params = {});

    /// Names of all registered non-null module keys.
    [[nodiscard]] static std::vector<std::string> availableModules();
};

}// namespace dl

#endif// WHISKERTOOLBOX_POST_ENCODER_MODULE_FACTORY_HPP
