/**
 * @file hdf5_utilities.cpp
 * @brief Non-template HDF5 utility implementations
 */

#include "hdf5_utilities.hpp"

#include <numeric>
#include <stdexcept>

namespace hdf5 {

namespace {

std::size_t compute_fast_axis_length(std::vector<hsize_t> const & dims, int sweep_row) {
    if (dims.empty()) {
        return 0;
    }

    if (dims.size() == 1) {
        return static_cast<std::size_t>(dims[0]);
    }

    if (dims.size() == 2) {
        if (sweep_row < 0 || static_cast<hsize_t>(sweep_row) >= dims[0]) {
            throw std::invalid_argument("sweep_row is out of range for 2D HDF5 dataset");
        }
        return static_cast<std::size_t>(dims[1]);
    }

    return static_cast<std::size_t>(
            std::accumulate(dims.begin(), dims.end(), static_cast<hsize_t>(1), std::multiplies<>{}));
}

}// namespace

std::vector<hsize_t> get_ragged_dims(H5::DataSet & dataset) {
    H5::DataSpace const dataspace = dataset.getSpace();
    int const n_dims = dataspace.getSimpleExtentNdims();
    std::vector<hsize_t> dims(static_cast<std::size_t>(n_dims));
    dataspace.getSimpleExtentDims(dims.data());
    return dims;
}

std::vector<hsize_t> get_dataset_dims(H5::DataSet & dataset) {
    return get_ragged_dims(dataset);
}

std::vector<hsize_t> get_dataset_dims(HDF5LoadOptions const & opts) {
    H5::H5File const file(opts.filepath.c_str(), H5F_ACC_RDONLY);
    H5::DataSet dataset = file.openDataSet(opts.key);
    return get_dataset_dims(dataset);
}

std::optional<std::size_t> get_identity_sample_count(HDF5FlatUnsignedOptions const & opts) {
    try {
        auto const dims = get_dataset_dims(HDF5LoadOptions{opts.filepath, opts.key});
        if (dims.empty()) {
            return std::nullopt;
        }
        return compute_fast_axis_length(dims, opts.sweep_row);
    } catch (std::exception const &) {
        return std::nullopt;
    }
}

std::vector<uint16_t> load_flat_unsigned_array(HDF5FlatUnsignedOptions const & opts) {
    H5::H5File const file(opts.filepath.c_str(), H5F_ACC_RDONLY);
    H5::DataSet dataset = file.openDataSet(opts.key);

    auto const dims = get_dataset_dims(dataset);
    if (dims.empty()) {
        throw std::runtime_error("HDF5 dataset is empty: " + opts.key);
    }

    H5::DataType const native_type = dataset.getDataType();
    bool const is_uint8 = native_type == H5::PredType::NATIVE_UINT8;
    bool const is_uint16 = native_type == H5::PredType::NATIVE_UINT16;
    if (!is_uint8 && !is_uint16) {
        throw std::runtime_error("HDF5 dataset must be uint8 or uint16 for bit-packed digital loading: " + opts.key);
    }

    std::size_t const sample_count = compute_fast_axis_length(dims, opts.sweep_row);
    std::vector<uint16_t> output;
    output.resize(sample_count);

    if (dims.size() == 1) {
        if (is_uint8) {
            std::vector<uint8_t> raw(sample_count);
            dataset.read(raw.data(), H5::PredType::NATIVE_UINT8);
            for (std::size_t i = 0; i < sample_count; ++i) {
                output[i] = static_cast<uint16_t>(raw[i]);
            }
        } else {
            dataset.read(output.data(), H5::PredType::NATIVE_UINT16);
        }
        return output;
    }

    if (dims.size() == 2) {
        if (opts.sweep_row < 0 || static_cast<hsize_t>(opts.sweep_row) >= dims[0]) {
            throw std::invalid_argument("sweep_row is out of range for 2D HDF5 dataset");
        }

        std::vector<hsize_t> const offset = {static_cast<hsize_t>(opts.sweep_row), 0};
        std::vector<hsize_t> const count = {1, dims[1]};

        H5::DataSpace const file_space = dataset.getSpace();
        file_space.selectHyperslab(H5S_SELECT_SET, count.data(), offset.data());

        H5::DataSpace const memspace(1, &count[1]);
        if (is_uint8) {
            std::vector<uint8_t> raw(sample_count);
            dataset.read(raw.data(), H5::PredType::NATIVE_UINT8, memspace, file_space);
            for (std::size_t i = 0; i < sample_count; ++i) {
                output[i] = static_cast<uint16_t>(raw[i]);
            }
        } else {
            dataset.read(output.data(), H5::PredType::NATIVE_UINT16, memspace, file_space);
        }
        return output;
    }

    auto const total_elements = static_cast<std::size_t>(
            std::accumulate(dims.begin(), dims.end(), static_cast<hsize_t>(1), std::multiplies<>{}));
    if (is_uint8) {
        std::vector<uint8_t> raw(total_elements);
        dataset.read(raw.data(), H5::PredType::NATIVE_UINT8);
        output.resize(total_elements);
        for (std::size_t i = 0; i < total_elements; ++i) {
            output[i] = static_cast<uint16_t>(raw[i]);
        }
    } else {
        output.resize(total_elements);
        dataset.read(output.data(), H5::PredType::NATIVE_UINT16);
    }
    return output;
}

}// namespace hdf5
