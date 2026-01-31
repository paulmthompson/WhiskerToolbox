#ifndef HDF5_LOADERS_HPP
#define HDF5_LOADERS_HPP

#include <string>
#include <vector>

namespace Loader {

struct HDF5LoadOptions {
    std::string filepath;
    std::string key;
};

// Return by value - RVO and move semantics will optimize
std::vector<std::vector<float>> read_ragged_hdf5(HDF5LoadOptions const & opts);

std::vector<int> read_array_hdf5(HDF5LoadOptions const & opts);

} // namespace Loader

#endif// HDF5_LOADERS_HPP
