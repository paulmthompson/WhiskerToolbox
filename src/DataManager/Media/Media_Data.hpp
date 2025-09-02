#ifndef WHISKERTOOLBOX_MEDIA_DATA_HPP
#define WHISKERTOOLBOX_MEDIA_DATA_HPP

#include "CoreGeometry/ImageSize.hpp"
#include "Observer/Observer_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "ImageProcessor.hpp"
#include "MediaStorage.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

class MediaData : public ObserverData {
public:
    enum class MediaType {
        Video,
        Images,
        HDF5
    };

    // Use shared MediaStorage types
    using BitDepth = MediaStorage::BitDepth;
    using DataStorage = MediaStorage::ImageDataVariant;

    MediaData();

    virtual ~MediaData();

    /**
     * @brief Get the media type of this instance
     * @return The MediaType enum value
     */
    virtual MediaType getMediaType() const = 0;

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

    // ========== Bit Depth Management ==========
    
    /**
     * @brief Get the current bit depth of the media data
     * @return Current BitDepth enum value
     */
    [[nodiscard]] BitDepth getBitDepth() const { return _bit_depth; }
    
    /**
     * @brief Set the bit depth (can be used to override auto-detected depth)
     * @param depth The desired bit depth
     */
    void setBitDepth(BitDepth depth);
    
    /**
     * @brief Check if the media data is using 8-bit storage
     * @return true if using 8-bit, false if using 32-bit float
     */
    [[nodiscard]] bool is8Bit() const { return MediaStorage::is8Bit(_rawData); }
    
    /**
     * @brief Check if the media data is using 32-bit float storage
     * @return true if using 32-bit float, false if using 8-bit
     */
    [[nodiscard]] bool is32Bit() const { return MediaStorage::is32Bit(_rawData); }

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

    // ========== Data Access Methods ==========
    
    /**
     * @brief Get raw data as uint8_t (for 8-bit data or converted from 32-bit)
     * @param frame_number Frame number to load
     * @return Reference to uint8_t vector
     */
    std::vector<uint8_t> const & getRawData8(int frame_number);
    
    /**
     * @brief Get raw data as float (for 32-bit data or converted from 8-bit)
     * @param frame_number Frame number to load
     * @return Reference to float vector
     */
    std::vector<float> const & getRawData32(int frame_number);
    
    /**
     * @brief Get processed data as uint8_t (for 8-bit data or converted from 32-bit)
     * @param frame_number Frame number to load
     * @return Copy of uint8_t vector (since processing may modify it)
     */
    std::vector<uint8_t> getProcessedData8(int frame_number);
    
    /**
     * @brief Get processed data as float (for 32-bit data or converted from 8-bit)
     * @param frame_number Frame number to load
     * @return Copy of float vector (since processing may modify it)
     */
    std::vector<float> getProcessedData32(int frame_number);
    
    /**
     * @brief Get raw data as variant (native format)
     * @param frame_number Frame number to load
     * @return DataStorage variant containing the raw data
     */
    DataStorage const& getRawDataVariant(int frame_number);
    
    /**
     * @brief Get processed data as variant (native format)
     * @param frame_number Frame number to load
     * @return DataStorage variant containing the processed data
     */
    DataStorage getProcessedDataVariant(int frame_number);
    
    /**
     * @brief Legacy method - get raw data (defaults to 8-bit behavior for compatibility)
     * @param frame_number Frame number to load
     * @return Reference to uint8_t vector
     */
    std::vector<uint8_t> const & getRawData(int frame_number) { return getRawData8(frame_number); }
    
    /**
     * @brief Legacy method - get processed data (defaults to 8-bit behavior for compatibility)
     * @param frame_number Frame number to load
     * @return Copy of uint8_t vector
     */
    std::vector<uint8_t> getProcessedData(int frame_number) { return getProcessedData8(frame_number); }
    
    /**
     * @brief Set raw data from uint8_t vector
     * @param data The 8-bit data to set
     */
    void setRawData(MediaStorage::ImageData8 data);
    
    /**
     * @brief Set raw data from float vector (normalized to 0-255 range)
     * @param data The 32-bit float data to set
     */
    void setRawData(MediaStorage::ImageData32 data);

    // Image processing methods using ImageProcessor system
    /**
     * @brief Set the image processor backend (e.g., "opencv", "simd", etc.)
     * @param processor_name Name of the registered processor
     * @return true if the processor was successfully set, false otherwise
     */
    bool setImageProcessor(std::string const& processor_name);

    /**
     * @brief Get the current image processor backend name
     * @return Name of the current processor, or empty string if none set
     */
    std::string getImageProcessorName() const;

    /**
     * @brief Add a processing step using the current processor
     * @param key Unique identifier for the processing step
     * @param processor Generic processing function
     */
    void addProcessingStep(std::string const& key, std::function<void(void*)> processor);

    /**
     * @brief Remove a processing step
     * @param key Identifier of the processing step to remove
     */
    void removeProcessingStep(std::string const& key);

    /**
     * @brief Clear all processing steps
     */
    void clearProcessingSteps();

    /**
     * @brief Check if a processing step exists
     * @param key Identifier to check
     * @return true if the step exists, false otherwise
     */
    bool hasProcessingStep(std::string const& key) const;

    /**
     * @brief Get the number of processing steps
     * @return Number of steps in the processing chain
     */
    size_t getProcessingStepCount() const;

    // ========== Time Frame ==========

    /**
     * @brief Set the time frame
     * 
     * @param time_frame The time frame to set
     */
    void setTimeFrame(std::shared_ptr<TimeFrame> time_frame) { _time_frame = time_frame; }

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

    // Bit depth and data storage
    BitDepth _bit_depth = BitDepth::Bit8;
    DataStorage _rawData;
    DataStorage _processedData;
    
    // Temporary conversion buffers (to avoid repeated allocations)
    mutable MediaStorage::ImageData8 _temp_8bit_buffer;
    mutable MediaStorage::ImageData32 _temp_32bit_buffer;
    
    // Flexible processing system
    std::unique_ptr<ImageProcessing::ImageProcessor> _image_processor;
    std::string _processor_name;
    
    int _last_loaded_frame = -1;
    int _last_processed_frame = -1;

    std::shared_ptr<TimeFrame> _time_frame {nullptr};

    void _processData();
    
    // Helper methods for data conversion
    void _convertTo8Bit(MediaStorage::ImageData32 const& source, MediaStorage::ImageData8& target) const;
    void _convertTo32Bit(MediaStorage::ImageData8 const& source, MediaStorage::ImageData32& target) const;
    
    // Helper to resize data storage based on current format and bit depth
    void _resizeDataStorage();
};

/**
 * @brief Empty MediaData implementation for default/placeholder use
 * 
 * This class provides a concrete implementation of MediaData that can be
 * used as a default/placeholder when no specific media type is loaded.
 */
class EmptyMediaData : public MediaData {
public:
    EmptyMediaData() = default;
    
    MediaType getMediaType() const override { 
        // Return Video as a default - this matches the old behavior
        // where MediaData defaulted to Video type detection
        return MediaType::Video; 
    }
    
protected:
    void doLoadMedia(std::string const& name) override {
        // No-op for empty media data
        static_cast<void>(name);
    }
    
    void doLoadFrame(int frame_id) override {
        // No-op for empty media data
        static_cast<void>(frame_id);
    }
};


#endif//WHISKERTOOLBOX_MEDIA_DATA_HPP
