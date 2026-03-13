#ifndef HDF5_UTILITIES_HPP
#define HDF5_UTILITIES_HPP

#include <iostream>
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

} // hdf5

// Include template implementations
#include "hdf5_utilities_impl.hpp"

#endif// HDF5_UTILITIES_HPP
