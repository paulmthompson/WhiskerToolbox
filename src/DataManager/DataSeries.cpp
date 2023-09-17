
#include "DataSeries.h"

#include <QImage>

#include <iostream>

namespace fs = std::filesystem;

std::vector<uint8_t> MediaSeries::getCurrentFrame() const {
    return this->current_frame;
}

int MediaSeries::LoadMedia(std::string name) {

    this->total_frame_count = this->doLoadMedia(name);
    return this->total_frame_count;
}

//Jump to specific frame designated by frame_id
int MediaSeries::LoadFrame(int frame_id , bool relative)
{

    if (relative) {
        frame_id = this->last_loaded_frame + frame_id;
    }

    if (frame_id < 0) {
        frame_id = 0;
    } else if (frame_id >= this->total_frame_count - 1) {
        frame_id = this->total_frame_count -1;
    }

    frame_id = doLoadFrame(frame_id);

    //UpdateCanvas(this->myimage);

    this->last_loaded_frame = frame_id;
    return this->last_loaded_frame;
    // I should emit a signal here that can be caught by anything that draws to scene (before or after draw? or both?)
}

int MediaSeries::getLastLoadedFrame() const {
    return last_loaded_frame;
}

int MediaSeries::findNearestSnapFrame(int frame) const {
    return doFindNearestSnapFrame(frame);
}

std::string MediaSeries::getFrameID(int frame) {
    return doGetFrameID(frame);
}

std::pair<int,int> MediaSeries::getMediaDimensions() const {
    return this-> doGetMediaDimensions();
}


///////////////////////////////////////////////////////////////////////////////////

int ImageSeries::doLoadMedia(std::string dir_name) {

    auto file_extension = ".png";

    for (const auto & entry : fs::directory_iterator(dir_name)) {
        if (entry.path().extension() == file_extension) {
            this->image_paths.push_back(dir_name / entry.path());
        }
    }

    // Load first testimage to get height and width
    auto myimage = QImage(QString::fromStdString(this->image_paths[0].string()));

    this->height = myimage.height();
    this->width = myimage.width();

    return this->image_paths.size();
}

int ImageSeries::doLoadFrame(int frame_id) {

    auto myimage = QImage(QString::fromStdString(this->image_paths[frame_id].string()));

    myimage.convertTo(QImage::Format_Grayscale8);

    this->current_frame = std::vector<uint8_t>(myimage.bits(), myimage.bits() + myimage.sizeInBytes());

    return frame_id;
}

int ImageSeries::doFindNearestSnapFrame(int frame_id) const {

    return frame_id;
}

std::string ImageSeries::doGetFrameID(int frame_id) {
    return this->image_paths[frame_id].filename().string();
}

std::pair<int,int> ImageSeries::doGetMediaDimensions() const {
    return std::pair<int,int>{this->height,this->width};
}


///////////////////////////////////////////////////////////////////////////////////

VideoSeries::VideoSeries() {
    vd = std::make_unique<ffmpeg_wrapper::VideoDecoder>();
}

int VideoSeries::GetVideoInfo(std::string name)
{
    this->data_filename = name;
    this->vd->createMedia(this->data_filename);

    this->current_frame.resize(vd->getHeight()*vd->getWidth());

    return vd->getFrameCount(); // Total frames
}

int VideoSeries::doLoadMedia(std::string name) {
    return this->GetVideoInfo(name);
}

int VideoSeries::doLoadFrame(int frame_id) {

    //In most circumstances, we want to decode forward from the current frame without reseeking to a keyframe
    bool frame_by_frame = true;

    if (frame_id == 0) {
        frame_by_frame = false;
    } else if (frame_id >= this->total_frame_count - 1) {
        frame_by_frame = false;
    } else if (frame_id <= this->last_loaded_frame) {
        frame_by_frame = false;
    }

    this->current_frame = vd->getFrame( frame_id, frame_by_frame);

    //auto image_native_resolution = QImage(&this->current_frame[0],vd->getWidth(), vd->getHeight(), QImage::Format_Grayscale8);

    //this->myimage = image_native_resolution.scaled(this->canvasWidth,this->canvasHeight);


    return frame_id;
}

int VideoSeries::doFindNearestSnapFrame(int frame_id) const {
    return this->vd->nearest_iframe(frame_id);
}

std::string VideoSeries::doGetFrameID(int frame_id) {
    return std::to_string(frame_id);
}

std::pair<int,int> VideoSeries::doGetMediaDimensions() const {
    return std::pair<int,int>{vd->getHeight(),vd->getWidth()};
}
