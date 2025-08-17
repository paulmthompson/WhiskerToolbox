#include "Media/MediaDataFactory.hpp"
#include "Media/Video_Data.hpp"
#include "Media/Video_Data_Loader.hpp"

namespace {
    // Register VideoData creator
    REGISTER_MEDIA_TYPE(Video, []() -> std::shared_ptr<MediaData> {
        return std::make_shared<VideoData>();
    });

    // Register VideoData loader
    REGISTER_MEDIA_LOADER(Video, [](std::string const& file_path, nlohmann::json const& config) -> std::shared_ptr<MediaData> {
        static_cast<void>(config); // Suppress unused parameter warning for now
        return load_video_into_VideoData(file_path);
    });
}
