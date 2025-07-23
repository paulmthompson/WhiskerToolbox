
#include "Mask_Data_JSON.hpp"

#include "Masks/Mask_Data.hpp"
#include "Masks/IO/HDF5/Mask_Data_HDF5.hpp"
#include "Masks/IO/Image/Mask_Data_Image.hpp"

#include "utils/json_helpers.hpp"
#include "loaders/loading_utils.hpp"


std::shared_ptr<MaskData> load_into_MaskData(std::string const & file_path, nlohmann::basic_json<> const & item) {

    if (!requiredFieldsExist(item,
                             {"format"},
                             "Error: Missing required field format. Supported options include hdf5, image"))
    {
        return std::make_shared<MaskData>();
    }

    auto const format = item["format"];

    if (format == "hdf5") {
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

    } else if (format == "image") {
        auto opts = ImageMaskLoaderOptions();
        opts.directory_path = file_path;
        
        // Parse optional image loading options
        if (item.contains("file_pattern")) {
            opts.file_pattern = item["file_pattern"];
        }
        if (item.contains("filename_prefix")) {
            opts.filename_prefix = item["filename_prefix"];
        }
        if (item.contains("frame_number_padding")) {
            opts.frame_number_padding = item["frame_number_padding"];
        }
        if (item.contains("threshold_value")) {
            opts.threshold_value = item["threshold_value"];
        }
        if (item.contains("invert_mask")) {
            opts.invert_mask = item["invert_mask"];
        }

        auto mask_data = load(opts);

        change_image_size_json(mask_data, item);

        return mask_data;

    } else {
        std::cerr << "Error: Unsupported format '" << format << "' for MaskData. Supported formats: hdf5, image" << std::endl;
        return std::make_shared<MaskData>();
    }
}
