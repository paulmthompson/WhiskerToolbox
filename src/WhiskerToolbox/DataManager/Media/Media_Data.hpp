//
// Created by wanglab on 4/10/2024.
//

#ifndef WHISKERTOOLBOX_MEDIA_DATA_HPP
#define WHISKERTOOLBOX_MEDIA_DATA_HPP

#include <vector>
#include <string>

#include "opencv2/opencv.hpp"


class MediaData {
public:

    MediaData();

    virtual ~MediaData();

    std::string getFilename() const {return _filename;};
    void setFilename(std::string const& filename) {_filename = filename;};

    enum DisplayFormat {
        Gray,
        Color
    };

    void setFormat(DisplayFormat const format);

    DisplayFormat getFormat() const {return _format;};

    int getHeight() const {return _height;};
    int getWidth() const {return _width;};

    void updateHeight(int const height);

    void updateWidth(int const width);

    int getTotalFrameCount() const {return _totalFrameCount;};
    void setTotalFrameCount(int const total_frame_count) {_totalFrameCount = total_frame_count;};

    /**
     *
     *
     *
     * @brief LoadMedia
     * @param name points to the path to the file or folder
     */
    void LoadMedia(std::string const& name);

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

    std::vector<uint8_t> const& getRawData(int const frame_number);
    void setRawData(std::vector<uint8_t> data) {_rawData = data;};

    std::vector<uint8_t> getProcessedData(const int frame_number);

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
    std::map<std::string, std::function<void(cv::Mat& input)>> _process_chain;

};

inline cv::Mat convert_vector_to_mat(std::vector<uint8_t>& vec, int const width, int const height);

#endif //WHISKERTOOLBOX_MEDIA_DATA_HPP
