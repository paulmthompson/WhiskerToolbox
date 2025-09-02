#include "media_export.hpp"

#include "DataManager/Media/Media_Data.hpp"
#include "DataManager/utils/string_manip.hpp"

#include <QImage>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>


std::string get_image_save_name(MediaData const * media, int const frame_id, MediaExportOptions const & opts) {
    
    if (media == nullptr) {
        std::cerr << "MediaData is nullptr" << std::endl;
        return "";
    }
    
    if (opts.save_by_frame_name) {
        auto saveName = media->GetFrameID(frame_id);
        return saveName;
    } else {

        std::string saveName = opts.image_name_prefix + pad_frame_id(frame_id, opts.frame_id_padding) + ".png";
        return saveName;
    }
}

void save_image(MediaData * media, int const frame_id, MediaExportOptions const & opts)
{
    std::filesystem::path save_dir = opts.image_save_dir;
    save_dir.append(opts.image_folder);
    //std::filesystem::path save_dir = opts.image_save_dir + opts.image_folder;
    if (!std::filesystem::exists(save_dir)) {
        std::filesystem::create_directory(save_dir);
        std::cout << "Created directory " << save_dir << std::endl;
    }

    auto saveName = get_image_save_name(media, frame_id, opts);
    auto full_save_path = save_dir / saveName;
    
    // Check if file exists and handle according to overwrite setting
    if (std::filesystem::exists(full_save_path) && !opts.overwrite_existing) {
        std::cout << "Skipping existing file: " << full_save_path.string() << std::endl;
        return;
    }
    
    // Log if we're overwriting an existing file
    if (std::filesystem::exists(full_save_path) && opts.overwrite_existing) {
        std::cout << "Overwriting existing file: " << full_save_path.string() << std::endl;
    }
    
    auto width = media->getWidth();
    auto height = media->getHeight();
    QImage labeled_image;
    
    // Handle different bit depths appropriately
    if (media->is8Bit()) {
        // 8-bit data: use Grayscale8 format
        auto image_8bit = media->getRawData8(frame_id);
        labeled_image = QImage(reinterpret_cast<const uchar*>(image_8bit.data()), 
                              width, height, QImage::Format_Grayscale8);
    } else if (media->is32Bit()) {
        // 32-bit float data: convert to 16-bit for higher precision saving
        auto image_32bit = media->getRawData32(frame_id);
        
        // Convert float data (already in 0-255 range) to 16-bit (0-65535 range)
        std::vector<uint16_t> image_16bit_converted;
        image_16bit_converted.reserve(image_32bit.size());
        
        for (float pixel_value : image_32bit) {
            // Scale from 0-255 range to 0-65535 range
            uint16_t value_16bit = static_cast<uint16_t>(pixel_value * 257.0f); // 257 = 65535/255
            image_16bit_converted.push_back(value_16bit);
        }
        
        labeled_image = QImage(reinterpret_cast<const uchar*>(image_16bit_converted.data()), 
                              width, height, width * sizeof(uint16_t), QImage::Format_Grayscale16);
    }
    
    labeled_image.save(QString::fromStdString(full_save_path.string()));
    std::cout << "Saved image to " << full_save_path.string() << std::endl;
}
