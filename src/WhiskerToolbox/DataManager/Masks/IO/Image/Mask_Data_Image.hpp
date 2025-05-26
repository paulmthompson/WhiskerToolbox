#ifndef MASK_DATA_IMAGE_HPP
#define MASK_DATA_IMAGE_HPP

#include <memory>
#include <string>

class MaskData;

/**
 * @struct ImageMaskLoaderOptions
 *
 * @brief Options for loading 2D MaskData from binary image files.
 *
 * This loader expects a directory containing binary (black and white) image files
 * where each file represents a mask for a specific frame. The filename should
 * contain the frame number, optionally with a prefix.
 *
 * @var ImageMaskLoaderOptions::directory_path
 * The directory containing the binary image files.
 *
 * @var ImageMaskLoaderOptions::file_pattern
 * File pattern to match (e.g., "*.png", "*.jpg", "*.bmp").
 *
 * @var ImageMaskLoaderOptions::filename_prefix
 * Optional prefix before the frame number in filenames (e.g., "mask_" for "mask_0001.png").
 *
 * @var ImageMaskLoaderOptions::frame_number_padding
 * Expected number of digits for frame numbers (e.g., 4 for "0001").
 * Set to 0 to disable padding requirements.
 *
 * @var ImageMaskLoaderOptions::threshold_value
 * Pixel intensity threshold for converting grayscale to binary (0-255).
 * Pixels >= threshold are considered mask pixels.
 *
 * @var ImageMaskLoaderOptions::invert_mask
 * If true, inverts the mask (white becomes background, black becomes mask).
 */
struct ImageMaskLoaderOptions {
    std::string directory_path = ".";
    std::string file_pattern = "*.png";
    std::string filename_prefix = "";
    int frame_number_padding = 0;
    int threshold_value = 128;
    bool invert_mask = false;
};

/**
 * @struct ImageMaskSaverOptions
 *
 * @brief Options for saving 2D MaskData to binary image files.
 *
 * This saver creates a directory containing binary (black and white) image files
 * where each file represents a mask for a specific frame. The filename will
 * contain the frame number, optionally with a prefix.
 *
 * @var ImageMaskSaverOptions::parent_dir
 * The directory where binary image files will be saved.
 *
 * @var ImageMaskSaverOptions::image_format
 * Image format to save (e.g., "PNG", "BMP", "TIFF").
 *
 * @var ImageMaskSaverOptions::filename_prefix
 * Optional prefix before the frame number in filenames (e.g., "mask_" for "mask_0001.png").
 *
 * @var ImageMaskSaverOptions::frame_number_padding
 * Number of digits for zero-padding frame numbers (e.g., 4 for "0001").
 *
 * @var ImageMaskSaverOptions::image_width
 * Width of the output images in pixels.
 *
 * @var ImageMaskSaverOptions::image_height
 * Height of the output images in pixels.
 *
 * @var ImageMaskSaverOptions::background_value
 * Pixel value for background (non-mask) pixels (0-255).
 *
 * @var ImageMaskSaverOptions::mask_value
 * Pixel value for mask pixels (0-255).
 */
struct ImageMaskSaverOptions {
    std::string parent_dir = ".";
    std::string image_format = "PNG";
    std::string filename_prefix = "";
    int frame_number_padding = 4;
    int image_width = 640;
    int image_height = 480;
    int background_value = 0;
    int mask_value = 255;
};

/**
 * @brief Load MaskData from binary image files
 *
 * Loads mask data from a directory of binary image files where each file
 * represents a mask for a specific frame. The filename should contain the
 * frame number, optionally with a prefix.
 *
 * Supported image formats: PNG, JPG, BMP, TIFF (any format supported by Qt's QImage)
 *
 * @param opts Options controlling the load behavior
 * @return A shared pointer to the loaded MaskData object
 */
std::shared_ptr<MaskData> load(ImageMaskLoaderOptions const & opts);

/**
 * @brief Save MaskData to binary image files
 *
 * Saves mask data to a directory of binary image files where each file
 * represents a mask for a specific frame. The filename will contain the
 * frame number, optionally with a prefix.
 *
 * @param mask_data The MaskData object to save
 * @param opts Options controlling the save behavior
 */
void save(MaskData const * mask_data, ImageMaskSaverOptions const & opts);

#endif// MASK_DATA_IMAGE_HPP 
