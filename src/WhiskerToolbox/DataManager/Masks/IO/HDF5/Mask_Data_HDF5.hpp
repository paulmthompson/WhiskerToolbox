#ifndef MASK_DATA_LOADER_HPP
#define MASK_DATA_LOADER_HPP


#include <memory>
#include <string>

class MaskData;

struct HDF5MaskLoaderOptions {
    std::string filename;
    std::string frame_key = "frames";
    std::string x_key = "widths";
    std::string y_key = "heights";
};

std::shared_ptr<MaskData> load(HDF5MaskLoaderOptions & opts);


#endif// MASK_DATA_LOADER_HPP
