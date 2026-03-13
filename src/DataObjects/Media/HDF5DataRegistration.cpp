#include "Media/MediaDataFactory.hpp"
#include "Media/HDF5_Data.hpp"

namespace {
    // Register HDF5Data creator
    REGISTER_MEDIA_TYPE(HDF5, []() -> std::shared_ptr<MediaData> {
        return std::make_shared<HDF5Data>();
    });

    // Note: HDF5 doesn't have a DM_DataType loader yet since it's typically
    // handled differently. This can be added when needed.
}
