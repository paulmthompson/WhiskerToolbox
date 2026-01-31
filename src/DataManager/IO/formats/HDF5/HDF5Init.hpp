#ifndef DATAMANAGER_IO_HDF5_INIT_HPP
#define DATAMANAGER_IO_HDF5_INIT_HPP

/**
 * @brief Initialize HDF5 loader registration
 * 
 * Call this function to ensure the HDF5 loader is registered with the
 * LoaderRegistry. This is needed because static registration might not
 * happen automatically in all build configurations.
 */
void initializeHDF5Loader();

#endif // DATAMANAGER_IO_HDF5_INIT_HPP
