/**
 * @file CloneMediaForInference.cpp
 * @brief Implementation of inference MediaData cloning.
 */

#include "CloneMediaForInference.hpp"

#include "Media/HDF5_Data.hpp"
#include "Media/Image_Data.hpp"
#include "Media/Media_Data.hpp"
#include "Media/Video_Data.hpp"

#include <spdlog/spdlog.h>

#include <memory>
#include <string>

std::shared_ptr<MediaData> cloneMediaForInference(MediaData const & source) {
    std::shared_ptr<MediaData> clone;

    switch (source.getMediaType()) {
    case MediaData::MediaType::Video: {
        auto video = std::make_shared<VideoData>();
        if (source.getFilename().empty()) {
            spdlog::error("cloneMediaForInference: video source has empty filename");
            return nullptr;
        }
        video->LoadMedia(source.getFilename());
        clone = std::move(video);
        break;
    }
    case MediaData::MediaType::Images: {
        auto images = std::make_shared<ImageData>();
        if (auto const * image_source = dynamic_cast<ImageData const *>(&source)) {
            if (!image_source->getFilename().empty()) {
                images->LoadMedia(image_source->getFilename());
            }
        } else if (!source.getFilename().empty()) {
            images->LoadMedia(source.getFilename());
        }
        clone = std::move(images);
        break;
    }
    case MediaData::MediaType::HDF5: {
        auto hdf5 = std::make_shared<HDF5Data>();
        if (source.getFilename().empty()) {
            spdlog::error("cloneMediaForInference: HDF5 source has empty filename");
            return nullptr;
        }
        hdf5->LoadMedia(source.getFilename());
        clone = std::move(hdf5);
        break;
    }
    default:
        spdlog::error("cloneMediaForInference: unsupported media type");
        return nullptr;
    }

    if (auto time_frame = source.getTimeFrame()) {
        clone->setTimeFrame(time_frame);
    }

    if (!source.getImageProcessorName().empty()) {
        clone->setImageProcessor(source.getImageProcessorName());
    }

    return clone;
}
