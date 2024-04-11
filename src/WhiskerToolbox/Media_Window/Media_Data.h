//
// Created by wanglab on 4/10/2024.
//

#ifndef WHISKERTOOLBOX_MEDIA_DATA_H
#define WHISKERTOOLBOX_MEDIA_DATA_H

#include <vector>
#include <string>

class MediaData {
public:

    std::string getFilename() const {return _filename;};
    void setFilename(std::string filename) {_filename = filename;};

    void setFormat(int format) {_format = format;};
    int getFormat() const {return _format;};

    int getHeight() const {return _height;};
    int getWidth() const {return _width;};
    void updateHeight(int height) {_height = height;};
    void updateWidth(int width) {_width = width;};

    int getTotalFrameCount() const {return _totalFrameCount;};
    void setTotalFrameCount(int total_frame_count) {_totalFrameCount = total_frame_count;};

    std::vector<uint8_t> getData() const {return this->data;};

    virtual int LoadMedia(std::string name) {return 0;};
    virtual void LoadFrame(int frame_id) {};
    virtual std::string GetFrameID(int frame_id) {return "";};

protected:


    std::vector<uint8_t> data;
private:
    std::string _filename;
    int _totalFrameCount;

    int _height;
    int _width;
    int _format; // This corresponds to an enum. Here we will use QImage.
};

#endif //WHISKERTOOLBOX_MEDIA_DATA_H
