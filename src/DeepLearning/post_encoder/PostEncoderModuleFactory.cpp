/// @file PostEncoderModuleFactory.cpp
/// @brief Implementation of the PostEncoderModule factory.

#include "PostEncoderModuleFactory.hpp"

#include "GlobalAvgPoolModule.hpp"
#include "SpatialPointExtractModule.hpp"

namespace dl {

std::unique_ptr<PostEncoderModule>
PostEncoderModuleFactory::create(
        std::string const & module_name,
        ImageSize source_image_size,
        SpatialPointModuleParams const & params) {

    if (module_name == "none" || module_name.empty()) {
        return nullptr;
    }

    if (module_name == "global_avg_pool") {
        return std::make_unique<GlobalAvgPoolModule>();
    }

    if (module_name == "spatial_point") {
        return std::make_unique<SpatialPointExtractModule>(
                source_image_size, params.interpolation);
    }

    return nullptr;
}

std::vector<std::string> PostEncoderModuleFactory::availableModules() {
    return {"global_avg_pool", "spatial_point"};
}

}// namespace dl
