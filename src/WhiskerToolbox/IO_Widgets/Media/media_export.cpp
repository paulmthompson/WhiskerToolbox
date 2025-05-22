#include "media_export.hpp"

#include "DataManager/Media/Media_Data.hpp"
#include "DataManager/utils/string_manip.hpp"

#include <QImage>

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
    auto saveName = get_image_save_name(media, frame_id, opts);
    auto image = media->getRawData(frame_id);
    auto width = media->getWidth();
    auto height = media->getHeight();
    QImage const labeled_image(&image[0], width, height, QImage::Format_Grayscale8);
    labeled_image.save(QString::fromStdString(opts.image_save_dir + saveName));
    std::cout << "Saved image to " << opts.image_save_dir + saveName << std::endl;
}
