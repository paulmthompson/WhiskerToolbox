#include "TimeScrollBar.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "DataManager/Media/Video_Data.hpp"

int TimeScrollBar::_getSnapFrame(int current_frame) {
    auto media = _data_manager->getData<MediaData>("media");
    if (media && media->getMediaType() == MediaData::MediaType::Video) {
        // Use dynamic_cast to get VideoData for snap functionality
        if (auto video_data = dynamic_cast<VideoData*>(media.get())) {
            return video_data->FindNearestSnapFrame(current_frame);
        }
    }
    // No snapping for non-video media or if cast fails
    return current_frame;
}
