#ifndef VIDEO_WINDOW_H
#define VIDEO_WINDOW_H

#include "Media_Window.h"

#include <ffmpeg_wrapper/videodecoder.h>

#include <memory>
#include <string>

class Video_Window : public Media_Window
{
 Q_OBJECT
public:
    Video_Window(QObject *parent = 0);

private:
    int doLoadMedia(std::string name) override;
    void doLoadFrame(int frame_id) override;
    int doFindNearestSnapFrame(int frame_id) const override;
    std::string doGetFrameID(int frame_id) override;
    std::pair<int,int> doGetMediaDimensions() const override;

    int GetVideoInfo(std::string name);

    std::unique_ptr<ffmpeg_wrapper::VideoDecoder> vd;

};
#endif // VIDEO_WINDOW_H
