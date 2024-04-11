//
// Created by wanglab on 4/10/2024.
//

#ifndef WHISKERTOOLBOX_VIDEO_DATA_HPP
#define WHISKERTOOLBOX_VIDEO_DATA_HPP

#include <string>
#include <memory>
#include <ffmpeg_wrapper/videodecoder.h>
#include "Media_Data.h"
#include "Media_Window.h"

class VideoData : public MediaData {
public:
    VideoData();

    int LoadMedia(std::string name) override;
    void LoadFrame(int frame_id) override;
    std::string GetFrameID(int frame_id) override;

    /**
     * When scrolling through large video files, it makes for much
     * smoother scrolling for the slider to "snap" or seek only
     * to key frames.
     *
     * @param frame_id
     * @return Nearest keyframe to frame_id
     */
    int FindNearestSnapFrame(int frame_id) const;
protected:

private:
    int _last_decoded_frame;
    std::unique_ptr<ffmpeg_wrapper::VideoDecoder> _vd;
};


#endif //WHISKERTOOLBOX_VIDEO_DATA_HPP
