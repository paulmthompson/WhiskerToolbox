#include "media_export.hpp"

#include "DataManager/Media/Media_Data.hpp"
#include "DataManager/utils/string_manip.hpp"

#include <QImage>

#include <algorithm>
#include <cstdint>
#include <cstring>
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
    if (width <= 0 || height <= 0) {
        std::cerr << "save_image: invalid image size " << width << "x" << height << std::endl;
        return;
    }
    QImage labeled_image;

    // Handle different bit depths appropriately
    if (media->is8Bit()) {
        // 8-bit data: use Grayscale8 format
        auto image_8bit = media->getRawData8(frame_id);
        std::size_t const expected = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        if (image_8bit.size() < expected) {
            std::cerr << "save_image: 8-bit buffer too small (" << image_8bit.size() << ") expected " << expected << std::endl;
            return;
        }
        QImage image(width, height, QImage::Format_Grayscale8);
        if (image.isNull()) {
            std::cerr << "save_image: failed to allocate 8-bit QImage" << std::endl;
            return;
        }
        int const src_bpl = width; // 1 byte per pixel
        for (int y = 0; y < height; ++y) {
            unsigned char * dst = static_cast<unsigned char*>(image.scanLine(y));
            std::size_t const src_start = static_cast<std::size_t>(y) * static_cast<std::size_t>(src_bpl);
            std::copy_n(image_8bit.data() + src_start, static_cast<std::size_t>(src_bpl), dst);
        }
        labeled_image = std::move(image);
    } else if (media->is32Bit()) {
        // 32-bit float data: convert to 16-bit for higher precision saving
        auto image_32bit = media->getRawData32(frame_id);
        std::size_t const expected = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        if (image_32bit.size() < expected) {
            std::cerr << "save_image: 32-bit buffer too small (" << image_32bit.size() << ") expected " << expected << std::endl;
            return;
        }

        // Convert float data (assumed 0-255) to 16-bit (0-65535)
        std::vector<uint16_t> image_16bit_converted;
        image_16bit_converted.reserve(image_32bit.size());

        constexpr float kU8ToU16Scale = 257.0f; // 65535/255
        for (float const pixel_value : image_32bit) {
            float const clamped = std::clamp(pixel_value, 0.0f, 255.0f);
            auto const value_16bit = static_cast<uint16_t>(clamped * kU8ToU16Scale);
            image_16bit_converted.push_back(value_16bit);
        }

        QImage image(width, height, QImage::Format_Grayscale16);
        if (image.isNull()) {
            std::cerr << "save_image: failed to allocate 16-bit QImage" << std::endl;
            return;
        }
        std::size_t const src_bpl = static_cast<std::size_t>(width) * sizeof(uint16_t);
        for (int y = 0; y < height; ++y) {
            uint16_t * dst = static_cast<uint16_t*>(static_cast<void*>(image.scanLine(y)));
            std::size_t const src_start = static_cast<std::size_t>(y) * static_cast<std::size_t>(width);
            std::copy_n(image_16bit_converted.data() + src_start, static_cast<std::size_t>(width), dst);
        }
        labeled_image = std::move(image);
    } else {
        std::cerr << "save_image: Unsupported media bit depth (not 8 or 32 bit)." << std::endl;
        return;
    }

    if (labeled_image.isNull()) {
        std::cerr << "save_image: Constructed QImage is null; aborting save." << std::endl;
        return;
    }

    bool ok = labeled_image.save(QString::fromStdString(full_save_path.string()));
    if (!ok && labeled_image.format() == QImage::Format_Grayscale16) {
        // Fallback: down-convert to 8-bit and try again
        QImage fallback8(width, height, QImage::Format_Grayscale8);
        if (!fallback8.isNull()) {
            for (int y = 0; y < height; ++y) {
                unsigned char * dst = static_cast<unsigned char*>(fallback8.scanLine(y));
                uint16_t const * src = reinterpret_cast<const uint16_t*>(labeled_image.constScanLine(y));
                for (int x = 0; x < width; ++x) {
                    dst[x] = static_cast<unsigned char>(src[x] / 257u);
                }
            }
            ok = fallback8.save(QString::fromStdString(full_save_path.string()));
            labeled_image = std::move(fallback8);
        }
    }
    if (ok) {
        std::cout << "Saved image to " << full_save_path.string() << std::endl;
    } else {
        std::cerr << "Failed to save image to " << full_save_path.string() << std::endl;
    }
}
