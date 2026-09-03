#ifndef HDF5_UTILITIES_HPP
#define HDF5_UTILITIES_HPP

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <H5Cpp.h>

// Much appreciation to comments provided here for ragged array loading
// https://github.com/BlueBrain/HighFive/issues/369#issuecomment-961133649

namespace hdf5 {

/**
 * @brief Get the dimensions of a dataset for ragged arrays
 * @param dataset HDF5 dataset
 * @return Vector of dimensions
 */
std::vector<hsize_t> get_ragged_dims(H5::DataSet & dataset);

/**
 * @brief Get the variable length type for template type T
 * @tparam T The data type (float, double, int)
 * @return H5::VarLenType for the specified type
 */
template<typename T>
H5::VarLenType get_varlen_type();

/**
 * @brief Get the HDF5 predefined type for template type T
 * @tparam T The data type (float, double, int)
 * @return H5::PredType for the specified type
 */
template<typename T>
H5::PredType get_datatype();

/**
 * @brief Load a ragged array from an HDF5 dataset
 * @tparam T The data type (float, double, int)
 * @param dataset HDF5 dataset
 * @return Vector of vectors containing the ragged array data
 */
template<typename T>
std::vector<std::vector<T>> load_ragged_array(H5::DataSet & dataset);

/**
 * @brief Load a regular array from an HDF5 dataset
 * @tparam T The data type (float, double, int)
 * @param dataset HDF5 dataset
 * @return Vector containing the array data
 */
template<typename T>
std::vector<T> load_array(H5::DataSet & dataset);

struct HDF5LoadOptions {
    std::string filepath;
    std::string key;
};

/**
 * @brief Options for loading flat unsigned sample arrays (e.g. Wavesurfer digitalScans)
 */
struct HDF5FlatUnsignedOptions {
    std::string filepath;
    std::string key;
    int sweep_row = 0;
};

/**
 * @brief Get the dimensions of an HDF5 dataset
 * @param dataset HDF5 dataset handle
 * @return Vector of dimension sizes
 */
std::vector<hsize_t> get_dataset_dims(H5::DataSet & dataset);

/**
 * @brief Get the dimensions of an HDF5 dataset by file path and key
 * @param opts Load options containing file path and dataset key
 * @return Vector of dimension sizes
 */
std::vector<hsize_t> get_dataset_dims(HDF5LoadOptions const & opts);

/**
 * @brief Compute the number of samples in a dataset for identity TimeFrame creation
 * @param opts Flat unsigned load options (uses sweep_row for 2D datasets)
 * @return Number of samples along the fast axis, or nullopt on error
 */
std::optional<std::size_t> get_identity_sample_count(HDF5FlatUnsignedOptions const & opts);

/**
 * @brief Load a flat unsigned array from an HDF5 dataset as uint16 samples
 *
 * Supports native uint8 (zero-extended) and uint16 storage. For 2D datasets,
 * reads the row specified by sweep_row. For 1D datasets, reads all elements.
 *
 * @param opts Flat unsigned load options
 * @return Vector of uint16 samples
 */
std::vector<uint16_t> load_flat_unsigned_array(HDF5FlatUnsignedOptions const & opts);

/**
 * @brief Load a regular array from an HDF5 file
 * @tparam T The data type (float, double, int)
 * @param opts Load options containing file path and key
 * @return Vector containing the array data
 */
template<typename T>
std::vector<T> load_array(HDF5LoadOptions const & opts);

/**
 * @brief Load a ragged array from an HDF5 file
 * @tparam T The data type (float, double, int)
 * @param opts Load options containing file path and key
 * @return Vector of vectors containing the ragged array data
 */
template<typename T>
std::vector<std::vector<T>> load_ragged_array(HDF5LoadOptions const & opts);

}// namespace hdf5

// Include template implementations
#include "hdf5_utilities_impl.hpp"

#endif// HDF5_UTILITIES_HPP
