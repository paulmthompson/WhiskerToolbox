#ifndef HDF5_MASK_LOAD_HPP
#define HDF5_MASK_LOAD_HPP

#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include <H5Cpp.h>

// Much appreciation to comments provided here for ragged array loading
// https://github.com/BlueBrain/HighFive/issues/369#issuecomment-961133649

/**
 *
 *
 *
 * @brief get_ragged_dims
 * @param dataset
 * @return
 */
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

template<typename T>
H5::VarLenType get_varlen_type() {
    H5::VarLenType mem_type;
    if constexpr (std::is_same_v<T, float>) {
        H5::FloatType const mem_item_type(H5::PredType::NATIVE_FLOAT);
        mem_type = H5::VarLenType(mem_item_type);
    } else if constexpr (std::is_same_v<T, double>) {
        H5::FloatType const mem_item_type(H5::PredType::NATIVE_DOUBLE);
        mem_type = H5::VarLenType(mem_item_type);
    } else if constexpr (std::is_same_v<T, int>) {
        H5::IntType const mem_item_type(H5::PredType::NATIVE_INT32);
        mem_type = H5::VarLenType(mem_item_type);
    }

    return mem_type;
}

template<typename T>
H5::PredType get_datatype() {
    if constexpr (std::is_same_v<T, float>) {
        return H5::PredType::NATIVE_FLOAT;
    } else if constexpr (std::is_same_v<T, double>) {
        return H5::PredType::NATIVE_DOUBLE;
    } else if constexpr (std::is_same_v<T, int>) {
        return H5::PredType::NATIVE_INT32;
    }
}

template<typename T>
std::vector<std::vector<T>> load_ragged_array(H5::DataSet & dataset) {
    auto dims = get_ragged_dims(dataset);

    hsize_t const n_rows = dims[0];
    std::vector<hvl_t> varlen_specs(n_rows);

    std::vector<std::vector<T>> data;
    data.reserve(n_rows);

    auto mem_type = get_varlen_type<T>();

    dataset.read(static_cast<void *>(varlen_specs.data()), mem_type);

    for (auto const & varlen_spec: varlen_specs) {
        auto data_ptr = static_cast<T *>(varlen_spec.p);
        data.emplace_back(data_ptr, data_ptr + varlen_spec.len);
        H5free_memory(varlen_spec.p);
    }

    return data;
}

template<typename T>
std::vector<T> load_array(H5::DataSet & dataset) {
    auto dims = get_ragged_dims(dataset);
    hsize_t const n_rows = dims[0];

    auto data = std::vector<T>(n_rows);

    std::cout << "Vector has " << data.size() << " elements";

    dataset.read(static_cast<void *>(data.data()), get_datatype<T>());

    return data;
}

template<typename T>
std::vector<T> load_array(std::string const & filepath, std::string const & key) {
    auto c_str = filepath.c_str();
    H5::H5File file(c_str, H5F_ACC_RDONLY);

    for (int i = 0; i < file.getNumObjs(); i++) {
        std::cout << file.getObjnameByIdx(i) << std::endl;
    }

    H5::DataSet dataset{file.openDataSet(key)};

    auto data = load_array<T>(dataset);

    file.close();

    return data;
}

template<typename T>
std::vector<std::vector<T>> load_ragged_array(std::string const & filepath, std::string const & key) {
    auto c_str = filepath.c_str();
    H5::H5File file(c_str, H5F_ACC_RDONLY);

    for (int i = 0; i < file.getNumObjs(); i++) {
        std::cout << file.getObjnameByIdx(i) << std::endl;
    }

    H5::DataSet dataset{file.openDataSet(key)};

    auto data = load_ragged_array<T>(dataset);

    file.close();

    return data;
}


#endif// HDF5_MASK_LOAD_HPP
