#ifndef IMAGE_DATA_LOADER_HPP
#define IMAGE_DATA_LOADER_HPP

#include "Media/Image_Data.hpp"

#include <memory>
#include <set>
#include <string>

/**
 * @struct ImageLoaderOptions
 * 
 * @brief Options for loading ImageData from a directory of image files.
 *          
 * @var ImageLoaderOptions::directory_path
 * The directory containing the image files to load.
 * 
 * @var ImageLoaderOptions::file_extensions
 * Set of file extensions to include (e.g., {".png", ".jpg", ".jpeg"}).
 * 
 * @var ImageLoaderOptions::filename_pattern
 * Optional regex pattern to search within filenames (e.g., "Ch1" to match files containing "Ch1").
 * Uses regex_search, so the pattern can match anywhere in the filename.
 * If empty, all files with matching extensions are included.
 * 
 * @var ImageLoaderOptions::sort_by_name
 * If true, files are sorted alphabetically by filename.
 * If false, files are processed in filesystem order.
 * 
 * @var ImageLoaderOptions::display_format
 * The display format for loaded images (Gray or Color).
 * 
 * @var ImageLoaderOptions::recursive_search
 * If true, search recursively in subdirectories.
 * If false, only search in the specified directory.
 */
struct ImageLoaderOptions {
    std::string directory_path = ".";
    std::set<std::string> file_extensions = {".png", ".jpg", ".jpeg"};
    std::string filename_pattern = "";
    bool sort_by_name = true;
    MediaData::DisplayFormat display_format = MediaData::DisplayFormat::Color;
    bool recursive_search = false;
};

/**
 * @brief Load ImageData from a directory of image files
 *
 * Loads image data from a directory containing image files. The function
 * will scan the directory for files with matching extensions and optionally
 * apply filename pattern matching.
 *
 * Supported image formats: PNG, JPG, JPEG (any format supported by OpenCV)
 *
 * @param opts Options controlling the load behavior
 * @return A shared pointer to the loaded ImageData object
 */
std::shared_ptr<ImageData> load(ImageLoaderOptions const & opts);

#endif// IMAGE_DATA_LOADER_HPP