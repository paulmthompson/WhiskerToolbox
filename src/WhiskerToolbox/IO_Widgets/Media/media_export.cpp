#include "media_export.hpp"

#include "DataManager/Media/Media_Data.hpp"
#include "DataManager/utils/string_manip.hpp"

#include <QImage>

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
    auto image = media->getRawData(frame_id);
    auto width = media->getWidth();
    auto height = media->getHeight();
    QImage const labeled_image(&image[0], width, height, QImage::Format_Grayscale8);
    labeled_image.save(QString::fromStdString(full_save_path.string()));
    std::cout << "Saved image to " << full_save_path.string() << std::endl;
}
