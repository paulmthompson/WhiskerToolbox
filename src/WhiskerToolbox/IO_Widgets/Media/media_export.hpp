#ifndef MEDIA_EXPORT_HPP
#define MEDIA_EXPORT_HPP

#include <string>

class MediaData;

struct MediaExportOptions {
    bool save_by_frame_name = false;
    int frame_id_padding = 7;
    std::string image_name_prefix = "img";
    std::string image_save_dir;
    std::string image_folder = "images";
    bool overwrite_existing = false;
};

std::string get_image_save_name(MediaData const * media, int frame_id, MediaExportOptions const & opts);

void save_image(MediaData * media, int frame_id, MediaExportOptions const & opts);

#endif // MEDIA_EXPORT_HPP
