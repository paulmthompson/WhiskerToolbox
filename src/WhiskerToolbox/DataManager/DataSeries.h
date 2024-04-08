#ifndef DATASERIES_H
#define DATASERIES_H

#include <string>
#include <memory>

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

class MediaSeries : DataSeries {
public:

protected:

    int height;
    int width;

     //This should be generic template to handle images / videos that are >8 bit or multiple channels
    std::vector<uint8_t> current_frame;
};

class VideoSeries : MediaSeries {
public:
    VideoSeries();
protected:
    std::unique_ptr<ffmpeg_wrapper::VideoDecoder> vd;
};

class ImageSeries : MediaSeries {

};


#endif // DATASERIES_H
