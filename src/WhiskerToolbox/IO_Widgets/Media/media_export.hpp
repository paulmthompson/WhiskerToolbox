#ifndef MEDIA_EXPORT_HPP
#define MEDIA_EXPORT_HPP

#include <string>

class MediaData;

struct MediaExportOptions {
    bool save_by_frame_name = false;
    int frame_id_padding = 7;
    std::string image_name_prefix = "img";
};

std::string get_image_save_name(MediaData const * media, int const frame_id, MediaExportOptions const & opts);

#endif // MEDIA_EXPORT_HPP
