//
// Created by wanglab on 4/10/2024.
//

#include "Video_Data.hpp"

VideoData::VideoData() :
    _vd{std::make_unique<ffmpeg_wrapper::VideoDecoder>()},
    _last_decoded_frame{0}
{}

void VideoData::doLoadMedia(std::string name) {
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

void VideoData::LoadFrame(int frame_id) {

    //In most circumstances, we want to decode forward from
    // the current frame without reseeking to a keyframe
    bool frame_by_frame = true;

    if (frame_id == 0) {
        frame_by_frame = false;
    } else if (frame_id >= this->getTotalFrameCount() - 1) {
        frame_by_frame = false;
    } else if (frame_id <= _last_decoded_frame) {
        frame_by_frame = false;
    }

    //We load the data associated with the frame
    this->setRawData(_vd->getFrame( frame_id, frame_by_frame));
    _last_decoded_frame = frame_id;

}

std::string VideoData::GetFrameID(int frame_id) {
    return std::to_string(frame_id);
}

int VideoData::FindNearestSnapFrame(int frame_id) const {
    return _vd->nearest_iframe(frame_id);
}
