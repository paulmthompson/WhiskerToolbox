
#include "Mask_Data_HDF5.hpp"

#include "ImageSize/ImageSize.hpp"
#include "loaders/hdf5_loaders.hpp"
#include "Masks/Mask_Data.hpp"

std::shared_ptr<MaskData> load(HDF5MaskLoaderOptions & opts) {

    auto frames = Loader::read_array_hdf5({opts.filename,  opts.frame_key});
    // auto probs = Loader::read_ragged_hdf5({filename, "probs"}); // Probs not used currently
    auto x_coords = Loader::read_ragged_hdf5({opts.filename, opts.x_key});
    auto y_coords = Loader::read_ragged_hdf5({opts.filename, opts.y_key});

    auto mask_data_ptr = std::make_shared<MaskData>();

    for (std::size_t i = 0; i < frames.size(); i++) {
        mask_data_ptr->addAtTime(frames[i], x_coords[i], y_coords[i]);
    }

    return mask_data_ptr;
}
