//
// Created by wanglab on 4/10/2024.
//

#ifndef WHISKERTOOLBOX_MEDIA_DATA_H
#define WHISKERTOOLBOX_MEDIA_DATA_H

#include <vector>
#include <string>

class MediaData {
public:

    MediaData() {
        _format = DisplayFormat::Gray;
        _display_format_bytes = 1;
    };

    std::string getFilename() const {return _filename;};
    void setFilename(std::string filename) {_filename = filename;};

    enum DisplayFormat {
        Gray,
        Color
    };

    void setFormat(DisplayFormat format)
    {
        _format = format;
        switch(_format)
        {
        case DisplayFormat::Gray:
            _display_format_bytes = 1;
            break;
        case DisplayFormat::Color:
            _display_format_bytes = 4;
            break;
        default:
            _display_format_bytes = 1;
            break;
        }
        this->data.resize(_height * _width * _display_format_bytes);
    };
    DisplayFormat getFormat() const {return _format;};

    int getHeight() const {return _height;};
    int getWidth() const {return _width;};
    void updateHeight(int height)
    {
        _height = height;
        this->data.resize(_height * _width * _display_format_bytes);
    };
    void updateWidth(int width)
    {
        _width = width;
        this->data.resize(_height * _width * _display_format_bytes);
    };

    int getTotalFrameCount() const {return _totalFrameCount;};
    void setTotalFrameCount(int total_frame_count) {_totalFrameCount = total_frame_count;};

    std::vector<uint8_t> getData() const {return this->data;};

    /**
     *
     *
     *
     * @brief LoadMedia
     * @param name points to the path to the file or folder
     * @return The total number of frames in the media
     */
    virtual int LoadMedia(std::string name) {return 0;};

    /**
     *
     * Subclasses will specify how to load a specific frame given by frame_id
     * This will populate the data class member with a vector of raw uint8_t
     *
     *
     *
     * @brief LoadFrame
     * @param frame_id
     */
    virtual void LoadFrame(int frame_id) {};


    virtual std::string GetFrameID(int frame_id) {return "";};

protected:


    std::vector<uint8_t> data;
private:
    std::string _filename;
    int _totalFrameCount;

    int _height;
    int _width;
    DisplayFormat _format; // This corresponds to an enum. Here we will use QImage.
    int _display_format_bytes;
};

#endif //WHISKERTOOLBOX_MEDIA_DATA_H