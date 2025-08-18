#include "HDF5Init.hpp"

// Forward declaration to force linking
extern "C" void ensure_hdf5_loader_registration();

void initializeHDF5Loader() {
    // Call the function to ensure the object file containing the loader
    // registration is linked into the final executable
    ::ensure_hdf5_loader_registration();
}
