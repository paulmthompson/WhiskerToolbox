#ifndef MASK_DATA_LOADER_HPP
#define MASK_DATA_LOADER_HPP

#include "DataManager.hpp"
#include "ImageSize/ImageSize.hpp"
#include "Masks/Mask_Data.hpp"
#include "utils/hdf5_mask_load.hpp"// load_array, load_ragged_array

#include "nlohmann/json.hpp"

#include <memory>
#include <string>

inline std::shared_ptr<MaskData> load_into_MaskData(std::string const & file_path, nlohmann::basic_json<> const & item) {
    std::string const frame_key = item["frame_key"];
    std::string const prob_key = item["probability_key"];
    std::string const x_key = item["x_key"];
    std::string const y_key = item["y_key"];

    int const height = item.value("height", -1);
    int const width = item.value("width", -1);

    auto frames = load_array<int>(file_path, frame_key);
    auto probs = load_ragged_array<float>(file_path, prob_key);
    auto y_coords = load_ragged_array<float>(file_path, y_key);
    auto x_coords = load_ragged_array<float>(file_path, x_key);

    auto mask_data = std::make_shared<MaskData>();
    mask_data->setImageSize(ImageSize{.width = width, .height = height});

    for (std::size_t i = 0; i < frames.size(); i++) {
        auto frame = frames[i];
        auto prob = probs[i];
        auto x = x_coords[i];
        auto y = y_coords[i];
        mask_data->addMaskAtTime(frame, x, y);
    }

    return mask_data;
}

#endif// MASK_DATA_LOADER_HPP
