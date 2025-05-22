
#include "Mask_Data_HDF5.hpp"

#include "ImageSize/ImageSize.hpp"
#include "loaders/hdf5_loaders.hpp"
#include "Masks/Mask_Data.hpp"
#include "nlohmann/json.hpp"


std::shared_ptr<MaskData> load_into_MaskData(std::string const & file_path, nlohmann::basic_json<> const & item) {

    auto opts = HDF5MaskLoaderOptions();
    opts.filename = file_path;
    opts.frame_key = item["frame_key"];
    opts.x_key = item["x_key"];
    opts.y_key = item["y_key"];

    std::string const prob_key = item["probability_key"];

    int const height = item.value("height", -1);
    int const width = item.value("width", -1);

    auto mask_data = load_mask_from_hdf5(opts);

    mask_data->setImageSize(ImageSize{.width = width, .height = height});

    return mask_data;
}

std::shared_ptr<MaskData> load_mask_from_hdf5(HDF5MaskLoaderOptions & opts) {

    auto frames = Loader::read_array_hdf5({opts.filename,  "frames"});
    // auto probs = Loader::read_ragged_hdf5({filename, "probs"}); // Probs not used currently
    auto y_coords = Loader::read_ragged_hdf5({opts.filename, "heights"});
    auto x_coords = Loader::read_ragged_hdf5({opts.filename, "widths"});

    auto mask_data_ptr = std::make_shared<MaskData>();

    for (std::size_t i = 0; i < frames.size(); i++) {
        mask_data_ptr->addAtTime(frames[i], x_coords[i], y_coords[i]);
    }

    return mask_data_ptr;
}
