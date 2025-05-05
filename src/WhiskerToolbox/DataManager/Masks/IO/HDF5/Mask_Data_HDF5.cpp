
#include "Mask_Data_HDF5.hpp"

#include "../../../DataManager.hpp"
#include "../../../ImageSize/ImageSize.hpp"
#include "../../../loaders/hdf5_loaders.hpp"
#include "../../Mask_Data.hpp"
#include "nlohmann/json.hpp"


std::shared_ptr<MaskData> load_into_MaskData(std::string const & file_path, nlohmann::basic_json<> const & item) {
    std::string const frame_key = item["frame_key"];
    std::string const prob_key = item["probability_key"];
    std::string const x_key = item["x_key"];
    std::string const y_key = item["y_key"];

    int const height = item.value("height", -1);
    int const width = item.value("width", -1);

    auto frames = Loader::read_array_hdf5({file_path, frame_key});
    auto probs = Loader::read_ragged_hdf5({file_path, prob_key});
    auto y_coords = Loader::read_ragged_hdf5({file_path, y_key});
    auto x_coords = Loader::read_ragged_hdf5({file_path, x_key});

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
