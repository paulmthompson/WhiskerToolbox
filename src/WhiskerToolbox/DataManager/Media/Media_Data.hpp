#ifndef WHISKERTOOLBOX_MEDIA_DATA_HPP
#define WHISKERTOOLBOX_MEDIA_DATA_HPP

#include "CoreGeometry/ImageSize.hpp"
#include "Observer/Observer_Data.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace cv {
class Mat;
}

class MediaData : public ObserverData {
public:
    MediaData();

    virtual ~MediaData() = default;

    [[nodiscard]] std::string getFilename() const { return _filename; };
    void setFilename(std::string const & filename) { _filename = filename; };

    enum DisplayFormat {
        Gray,
        Color
    };

    void setFormat(DisplayFormat format);

    [[nodiscard]] DisplayFormat getFormat() const { return _format; };

    [[nodiscard]] int getHeight() const { return _height; };
    [[nodiscard]] int getWidth() const { return _width; };
    [[nodiscard]] ImageSize getImageSize() const {return ImageSize{.width=_width, .height=_height};};

    void updateHeight(int height);

    void updateWidth(int width);

    [[nodiscard]] int getTotalFrameCount() const { return _totalFrameCount; };
    void setTotalFrameCount(int total_frame_count) { _totalFrameCount = total_frame_count; };

    /**
     *
     *
     *
     * @brief LoadMedia
     * @param name points to the path to the file or folder
     */
    void LoadMedia(std::string const & name);

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
    void LoadFrame(int frame_id);

    [[nodiscard]] virtual std::string GetFrameID(int frame_id) const {
        static_cast<void>(frame_id);
        return "";
    };

    virtual int getFrameIndexFromNumber(int frame_id) {
        static_cast<void>(frame_id);
        return 0;
    };

    std::vector<uint8_t> const & getRawData(int frame_number);
    void setRawData(std::vector<uint8_t> data) { _rawData = std::move(data); };

    std::vector<uint8_t> getProcessedData(int frame_number);

    void setProcess(std::string const & key, std::function<void(cv::Mat & input)> process);
    void removeProcess(std::string const & key);

protected:
    virtual void doLoadMedia(std::string const & name) {
        static_cast<void>(name);
    };
    virtual void doLoadFrame(int frame_id) {
        static_cast<void>(frame_id);
    };

private:
    std::string _filename;
    int _totalFrameCount = 0;

    int _height = 480;
    int _width = 640;
    DisplayFormat _format = DisplayFormat::Gray;
    int _display_format_bytes = 1;

    std::vector<uint8_t> _rawData;
    std::vector<uint8_t> _processedData;
    std::map<std::string, std::function<void(cv::Mat & input)>> _process_chain;
    int _last_loaded_frame = -1;
    int _last_processed_frame = -1;

    void _processData();
};


#endif//WHISKERTOOLBOX_MEDIA_DATA_HPP
