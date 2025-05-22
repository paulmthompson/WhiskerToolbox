#ifndef MASK_DATA_LOADER_HPP
#define MASK_DATA_LOADER_HPP

#include "nlohmann/json.hpp"

#include <memory>
#include <string>

class MaskData;

struct HDF5MaskLoaderOptions {
    std::string filename;
    std::string frame_key = "frames";
    std::string x_key = "widths";
    std::string y_key = "heights";
};

std::shared_ptr<MaskData> load_into_MaskData(std::string const & file_path, nlohmann::basic_json<> const & item);

std::shared_ptr<MaskData> load_mask_from_hdf5(HDF5MaskLoaderOptions & opts);

#endif// MASK_DATA_LOADER_HPP
