#include "Media/MediaDataFactory.hpp"
#include "Media/Image_Data.hpp"
#include "Media/IO/JSON/Image_Data_JSON.hpp"

namespace {
    // Register ImageData creator
    REGISTER_MEDIA_TYPE(Images, []() -> std::shared_ptr<MediaData> {
        return std::make_shared<ImageData>();
    });

    // Register ImageData loader
    REGISTER_MEDIA_LOADER(Images, [](std::string const& file_path, nlohmann::json const& config) -> std::shared_ptr<MediaData> {
        return load_into_ImageData(file_path, config);
    });
}
