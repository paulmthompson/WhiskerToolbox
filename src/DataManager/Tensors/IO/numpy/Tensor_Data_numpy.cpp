
#include "Tensor_Data_numpy.hpp"
#include "../../TensorData.hpp"
#include "npy.hpp"

#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"

#include <filesystem>
#include <iostream>
#include <algorithm>
#include <memory>

#if defined(_WIN32) || defined(__APPLE__)
std::vector<long long> convertShape(const std::vector<unsigned long>& shape) {
    std::vector<long long> convertedShape(shape.size());
    std::transform(shape.begin(), shape.end(), convertedShape.begin(), [](unsigned long i) { return static_cast<long long>(i); });
    return convertedShape;
}
#else
std::vector<long> convertShape(const std::vector<unsigned long>& shape) {
    std::vector<long> convertedShape(shape.size());
    std::transform(shape.begin(), shape.end(), convertedShape.begin(), [](unsigned long i) { return static_cast<long>(i); });
    return convertedShape;
}
#endif

void loadNpyToTensorData(const std::string& filepath, TensorData& tensor_data) {
    if (!std::filesystem::exists(filepath)) {
        std::cout << "File does not exist: " << filepath << std::endl;
        return;
    }

    try {
        auto npy_data = npy::read_npy<float>(filepath);

        std::cout << "Loaded numpy tensor with shape: ";
        for (size_t i = 0; i < npy_data.shape.size(); ++i) {
            std::cout << npy_data.shape[i];
            if (i < npy_data.shape.size() - 1) std::cout << "x";
        }
        std::cout << std::endl;

        // Convert shape to size_t vector
        std::vector<std::size_t> full_shape;
        for (unsigned long dim : npy_data.shape) {
            full_shape.push_back(static_cast<std::size_t>(dim));
        }

        if (full_shape.empty()) {
            std::cout << "Empty tensor shape" << std::endl;
            return;
        }

        // Assuming the tensor is a multi-dimensional tensor where the first dimension is time
        auto const time_steps = full_shape[0];
        
        // Calculate the total number of features (flatten all dimensions after time)
        std::size_t num_features = 1;
        for (std::size_t i = 1; i < full_shape.size(); ++i) {
            num_features *= full_shape[i];
        }


        // Create TmeIndexStorage for dense sequential indices [0, 1, 2, ..., time_steps-1]
        auto time_storage = TimeIndexStorageFactory::createDenseFromZero(time_steps);

        // Create TensorData using the factory method
        // The numpy data is already in row-major format (time x features)
        tensor_data = TensorData::createTimeSeries2D(
            npy_data.data,      // flat vector of floats (already in row-major order)
            time_steps,         // num_rows (time dimension)
            num_features,       // num_cols (flattened feature dimensions)
            time_storage,       // time index mapping
            nullptr,         // time values
            {}                  // no column names
        );

        std::cout << "Loaded " << time_steps << " timestamps of tensors with "
                  << num_features << " features (";
        for (std::size_t i = 1; i < full_shape.size(); ++i) {
            std::cout << full_shape[i];
            if (i < full_shape.size() - 1) std::cout << "x";
        }
        std::cout << " flattened)" << std::endl;

    } catch (const std::exception& e) {
        std::cout << "Error loading tensor from file: " << e.what() << std::endl;
    }
}
