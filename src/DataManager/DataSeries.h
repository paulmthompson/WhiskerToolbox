#ifndef DATASERIES_H
#define DATASERIES_H

#include <string>
#include <memory>
#include <filesystem>

#include <ffmpeg_wrapper/videodecoder.h>

class DataSeries {
public:

//    getDataAtTime(int ind);
//    getDataInRange(int range_start, int range_end);
protected:

    std::string data_folder_path;
    std::string data_filename;

    int data_length;
};

class MediaSeries : public DataSeries {
public:

    std::vector<uint8_t> getCurrentFrame() const;

    int LoadMedia(std::string name);

    //Jump to specific frame designated by frame_id, or relative to last loaded frame if relative is true
    int LoadFrame(int frame_id, bool relative = false);

    int getLastLoadedFrame() const;

    int findNearestSnapFrame(int frame) const;
    std::string getFrameID(int frame);

    std::pair<int,int> getMediaDimensions() const;
    int getHeight() const {return height;};
    int getWidth() const {return width;};

protected:

    int height;
    int width;

    int total_frame_count;
    int last_loaded_frame;

     //This should be generic template to handle images / videos that are >8 bit or multiple channels
    std::vector<uint8_t> current_frame;

    virtual int doLoadMedia(std::string name) {return 0;};
    virtual int doLoadFrame(int frame_id) {return 0;};
    virtual int doFindNearestSnapFrame(int frame_id) const {return frame_id;};
    virtual std::string doGetFrameID(int frame_id) {return "";}; // This should be used with data structure
    virtual std::pair<int,int> doGetMediaDimensions() const {return std::pair<int,int>{0,0};};
};


///////////////////////////////////////////////////////////////////////////////////

class VideoSeries : public MediaSeries {
public:
    VideoSeries();

protected:
    int doLoadMedia(std::string name) override;
    int doLoadFrame(int frame_id) override;
    int doFindNearestSnapFrame(int frame_id) const override;
    std::string doGetFrameID(int frame_id) override;
    std::pair<int,int> doGetMediaDimensions() const override;

    std::unique_ptr<ffmpeg_wrapper::VideoDecoder> vd;
    int GetVideoInfo(std::string name);
};


///////////////////////////////////////////////////////////////////////////////////

class ImageSeries : public MediaSeries {
    int doLoadMedia(std::string name) override;
    int doLoadFrame(int frame_id) override;
    int doFindNearestSnapFrame(int frame_id) const override;
    std::string doGetFrameID(int frame_id) override;
    std::pair<int,int> doGetMediaDimensions() const override;

    std::vector<std::filesystem::path> image_paths;
};


#endif // DATASERIES_H
