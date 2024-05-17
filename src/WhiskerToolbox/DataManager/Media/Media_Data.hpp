//
// Created by wanglab on 4/10/2024.
//

#ifndef WHISKERTOOLBOX_MEDIA_DATA_HPP
#define WHISKERTOOLBOX_MEDIA_DATA_HPP

#include <vector>
#include <string>

#if defined _WIN32 || defined __CYGWIN__
    #define DLLOPT __declspec(dllexport)
    //#define DLLOPT __declspec(dllexport)
#else
    #define DLLOPT __attribute__((visibility("default")))
#endif

class DLLOPT MediaData {
public:

    MediaData();

    ~MediaData() = default;

    std::string getFilename() const {return _filename;};
    void setFilename(std::string filename) {_filename = filename;};

    enum DisplayFormat {
        Gray,
        Color
    };

    void setFormat(DisplayFormat format);

    DisplayFormat getFormat() const {return _format;};

    int getHeight() const {return _height;};
    int getWidth() const {return _width;};

    void updateHeight(int height);

    void updateWidth(int width);

    int getTotalFrameCount() const {return _totalFrameCount;};
    void setTotalFrameCount(int total_frame_count) {_totalFrameCount = total_frame_count;};

    /**
     *
     *
     *
     * @brief LoadMedia
     * @param name points to the path to the file or folder
     */
    void LoadMedia(std::string name);

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

    std::vector<uint8_t> getRawData(int frame_number);
    void setRawData(std::vector<uint8_t> data) {_rawData = data;};

protected:
    virtual void doLoadMedia(std::string name) {return;};
private:
    std::string _filename;
    int _totalFrameCount;

    int _height;
    int _width;
    DisplayFormat _format; // This corresponds to an enum. Here we will use QImage.
    int _display_format_bytes;

    std::vector<uint8_t> _rawData;
};

#endif //WHISKERTOOLBOX_MEDIA_DATA_HPP
