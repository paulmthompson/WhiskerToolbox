#include "Media/Video_Data.hpp"

#include "ffmpeg_wrapper/videodecoder.h"

VideoData::VideoData()
    : _vd{std::make_unique<ffmpeg_wrapper::VideoDecoder>()} {}

VideoData::~VideoData() = default;

void VideoData::doLoadMedia(std::string const & name) {
    setFilename(name);
    _vd->createMedia(name);

    updateHeight(_vd->getHeight());
    updateWidth(_vd->getWidth());

    switch (getFormat()) {
        case DisplayFormat::Gray:
            _vd->setFormat(ffmpeg_wrapper::VideoDecoder::Gray8);
            break;
        case DisplayFormat::Color:
            _vd->setFormat(ffmpeg_wrapper::VideoDecoder::ARGB);
            break;
        default:
            _vd->setFormat(ffmpeg_wrapper::VideoDecoder::Gray8);
    }

    // Set format of video decoder to the format currently
    // selected in the MediaData
    //setFormat(QImage::Format_Grayscale8);

    setTotalFrameCount(_vd->getFrameCount());
}

void VideoData::doLoadFrame(int frame_id) {
    // In most circumstances, we want to decode forward from
    // the current frame without reseeking to a keyframe
    bool frame_by_frame = true;

    // Direct seeking is needed when:
    // - Going to the start or end of video
    // - Going backwards
    // - Making large jumps forward (more than 100 frames ahead)
    if ((frame_id == 0) ||
        (frame_id >= this->getTotalFrameCount() - 1) ||
        (frame_id <= _last_decoded_frame) ||
        (frame_id > _last_decoded_frame + 100)) { // Add this condition for large forward jumps
        frame_by_frame = false;
    }

    // We load the data associated with the frame
    // Videos are typically 8-bit, so we use the 8-bit setRawData method
    this->setRawData(_vd->getFrame(frame_id, frame_by_frame));
    _last_decoded_frame = frame_id;
}

std::string VideoData::GetFrameID(int frame_id) const {
    return std::to_string(frame_id);
}

int VideoData::FindNearestSnapFrame(int frame_id) const {
    return static_cast<int>(_vd->nearest_iframe(frame_id));
}
