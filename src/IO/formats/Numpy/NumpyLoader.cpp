#include "NumpyLoader.hpp"
#include "tensordata/Tensor_Data_numpy.hpp"

#include <iostream>
#include <memory>

std::string NumpyLoader::getFormatId() const {
    return "numpy";
}

bool NumpyLoader::supportsDataType(DM_DataType data_type) const {
    using enum DM_DataType;
    switch (data_type) {
        case Tensor:
            return true;
        default:
            return false;
    }
}

LoadResult NumpyLoader::loadData(
        std::string const & file_path,
        DM_DataType data_type,
        nlohmann::json const & config) const {
    try {
        using enum DM_DataType;
        if (data_type == Tensor) {
            return loadTensorData(file_path, config);
        }
        return LoadResult("Unsupported data type for Numpy loader");
    } catch (std::exception const & e) {
        return LoadResult("Numpy loading error: " + std::string(e.what()));
    }
}

LoadResult NumpyLoader::loadTensorData(
        std::string const & file_path,
        [[maybe_unused]] nlohmann::json const & config) {
    try {
        NpyTensorLoaderOptions opts;
        opts.filepath = file_path;
        auto tensor_data = load(opts);

        return LoadResult(LoadedDataVariant{std::move(tensor_data)});

    } catch (std::exception const & e) {
        return LoadResult("Numpy TensorData loading failed: " + std::string(e.what()));
    }
}
