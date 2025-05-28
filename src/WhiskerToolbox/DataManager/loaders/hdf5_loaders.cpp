#include "hdf5_loaders.hpp"

#include "utils/hdf5_mask_load.hpp"
#include <vector>

namespace Loader {

std::vector<std::vector<float>> read_ragged_hdf5(HDF5LoadOptions const & opts) {
    return load_ragged_array<float>(reinterpret_cast<hdf5::HDF5LoadOptions const &>(opts));
}

std::vector<int> read_array_hdf5(HDF5LoadOptions const & opts) {
    return load_array<int>(reinterpret_cast<hdf5::HDF5LoadOptions const &>(opts));
}

} // Loader
