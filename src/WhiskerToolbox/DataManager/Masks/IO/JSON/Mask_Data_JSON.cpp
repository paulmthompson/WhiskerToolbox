
#include "Mask_Data_JSON.hpp"

#include "Masks/Mask_Data.hpp"
#include "Masks/IO/HDF5/Mask_Data_HDF5.hpp"

#include "utils/json_helpers.hpp"
#include "loaders/loading_utils.hpp"


std::shared_ptr<MaskData> load_into_MaskData(std::string const & file_path, nlohmann::basic_json<> const & item) {

    if (!requiredFieldsExist(item,
                             {"frame_key", "x_key", "y_key"},
                             "Error: Missing required fields in Mask Data"))
    {
        return std::make_shared<MaskData>();
    }

    auto opts = HDF5MaskLoaderOptions();
    opts.filename = file_path;
    opts.frame_key = item["frame_key"];
    opts.x_key = item["x_key"];
    opts.y_key = item["y_key"];

    //std::string const prob_key = item["probability_key"];

    auto mask_data = load(opts);

    change_image_size_json(mask_data, item);

    return mask_data;
}
