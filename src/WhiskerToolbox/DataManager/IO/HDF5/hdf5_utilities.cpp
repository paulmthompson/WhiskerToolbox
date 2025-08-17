#include "hdf5_utilities.hpp"

namespace hdf5 {

std::vector<hsize_t> get_ragged_dims(H5::DataSet & dataset) {
    H5::DataSpace const dataspace = dataset.getSpace();
    int const n_dims = dataspace.getSimpleExtentNdims();
    std::vector<hsize_t> dims(n_dims);
    dataspace.getSimpleExtentDims(dims.data());

    std::cout << "n_dims: " << dims.size() << '\n';

    std::cout << "shape: (";
    for (hsize_t const dim: dims) {
        std::cout << dim << ", ";
    }
    std::cout << ")\n"
              << std::endl;

    return dims;
}

} // hdf5
