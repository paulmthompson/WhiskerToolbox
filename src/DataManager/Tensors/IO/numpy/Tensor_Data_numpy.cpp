
#include "Tensor_Data_numpy.hpp"
#include "../../Tensor_Data.hpp"
#include "npy.hpp"

#include <filesystem>
#include <iostream>
#include <algorithm>

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
        
        // Calculate the size of each time slice
        std::size_t slice_size = 1;
        std::vector<std::size_t> feature_shape;
        for (std::size_t i = 1; i < full_shape.size(); ++i) {
            slice_size *= full_shape[i];
            feature_shape.push_back(full_shape[i]);
        }

        // Set the feature shape
        tensor_data.setFeatureShape(feature_shape);

        // Extract data for each time step
        for (std::size_t t = 0; t < time_steps; ++t) {
            // Extract slice data for this time step
            std::vector<float> slice_data;
            slice_data.reserve(slice_size);
            
            std::size_t start_idx = t * slice_size;
            std::size_t end_idx = start_idx + slice_size;
            
            for (std::size_t i = start_idx; i < end_idx && i < npy_data.data.size(); ++i) {
                slice_data.push_back(npy_data.data[i]);
            }

            // Add the slice to tensor data
            tensor_data.addTensorAtTime(TimeFrameIndex(t), slice_data, feature_shape);
        }

        std::cout << "Loaded " << time_steps << " timestamps of tensors with feature shape: ";
        for (std::size_t i = 0; i < feature_shape.size(); ++i) {
            std::cout << feature_shape[i];
            if (i < feature_shape.size() - 1) std::cout << "x";
        }
        std::cout << std::endl;

    } catch (const std::exception& e) {
        std::cout << "Error loading tensor from file: " << e.what() << std::endl;
    }
}
