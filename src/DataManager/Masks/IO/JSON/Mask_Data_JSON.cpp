
#include "Mask_Data_JSON.hpp"

#include "Masks/Mask_Data.hpp"

#include "utils/json_helpers.hpp"
#include "loaders/loading_utils.hpp"


std::shared_ptr<MaskData> load_into_MaskData(std::string const & file_path, nlohmann::basic_json<> const & item) {

    if (!requiredFieldsExist(item,
                             {"format"},
                             "Error: Missing required field format. Supported options include image"))
    {
        return std::make_shared<MaskData>();
    }

    auto const format = item["format"];

    if (format == "hdf5") {
        // For HDF5 format, delegate to factory to avoid circular dependency
        // The actual HDF5 loading will be handled by the DataManagerHDF5 plugin
        
        std::cerr << "Warning: HDF5 loading through JSON configuration requires DataManagerHDF5 plugin" << std::endl;
        std::cerr << "Returning empty MaskData. Use direct HDF5 loader instead." << std::endl;
        
        auto mask_data = std::make_shared<MaskData>();
        change_image_size_json(mask_data, item);
        return mask_data;

    } else if (format == "image") {
        // Image format is now handled by OpenCV plugin
        // This should be loaded through the plugin system
        std::cerr << "Warning: Image format should be loaded through plugin system, not JSON loader\n";
        return std::make_shared<MaskData>();

    } else {
        std::cerr << "Error: Unsupported format '" << format << "' for MaskData. Supported formats: image" << std::endl;
        std::cerr << "Note: HDF5 format requires direct loading through DataManagerHDF5 plugin" << std::endl;
        return std::make_shared<MaskData>();
    }
}
