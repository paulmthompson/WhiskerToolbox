
#include "hdf5_loaders.hpp"

#include "utils/hdf5_mask_load.hpp"

namespace Loader {

std::vector<std::vector<float>> read_ragged_hdf5(HDF5LoadOptions const & opts) {
    auto myvector = load_ragged_array<float>(reinterpret_cast<hdf5::HDF5LoadOptions const &>(opts));
    return myvector;
}

std::vector<int> read_array_hdf5(HDF5LoadOptions const & opts) {
    auto myvector = load_array<int>(reinterpret_cast<hdf5::HDF5LoadOptions const &>(opts));
    return myvector;
}

} // Loader
