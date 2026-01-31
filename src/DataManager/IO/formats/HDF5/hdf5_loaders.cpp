#include "hdf5_loaders.hpp"

#include "common/hdf5_utilities.hpp"
#include <vector>

namespace Loader {

// Helper function to convert between the two HDF5LoadOptions structs
static hdf5::HDF5LoadOptions convert_options(HDF5LoadOptions const & opts) {
    return {opts.filepath, opts.key};
}

std::vector<std::vector<float>> read_ragged_hdf5(HDF5LoadOptions const & opts) {
    return hdf5::load_ragged_array<float>(convert_options(opts));
}

std::vector<int> read_array_hdf5(HDF5LoadOptions const & opts) {
    return hdf5::load_array<int>(convert_options(opts));
}

} // namespace Loader
