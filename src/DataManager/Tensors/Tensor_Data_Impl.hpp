#ifndef TENSOR_DATA_IMPL_HPP
#define TENSOR_DATA_IMPL_HPP

#include "TimeFrame/TimeFrame.hpp"

#ifdef TENSOR_BACKEND_LIBTORCH
#include <torch/torch.h>
#else
#include <armadillo>
#endif

#include <map>
#include <vector>
#include <memory>

/**
 * @brief Implementation class for TensorData using PIMPL pattern
 * 
 * This class contains the actual implementation details and heavy dependencies
 * that are hidden from the public interface.
 */
class TensorDataImpl {
public:
    // ========== Constructors ==========
    /**
     * @brief Default constructor
     */
    TensorDataImpl() = default;

    /**
     * @brief Destructor
     */
    ~TensorDataImpl() = default;

    /**
     * @brief Copy constructor
     * @param other TensorDataImpl to copy from
     */
    TensorDataImpl(const TensorDataImpl& other);

    /**
     * @brief Move constructor
     * @param other TensorDataImpl to move from
     */
    TensorDataImpl(TensorDataImpl&& other) noexcept;

    /**
     * @brief Copy assignment operator
     * @param other TensorDataImpl to copy from
     * @return Reference to this TensorDataImpl
     */
    TensorDataImpl& operator=(const TensorDataImpl& other);

    /**
     * @brief Move assignment operator
     * @param other TensorDataImpl to move from
     * @return Reference to this TensorDataImpl
     */
    TensorDataImpl& operator=(TensorDataImpl&& other) noexcept;

#ifdef TENSOR_BACKEND_LIBTORCH
    /**
     * @brief Constructor for TensorDataImpl from a map of TimeFrameIndex to torch::Tensor
     * @param data Map of TimeFrameIndex to torch::Tensor
     * @param shape Vector of integers representing the shape of the tensors
     */
    template<typename T>
    TensorDataImpl(std::map<TimeFrameIndex, torch::Tensor> data, std::vector<T> shape);
#endif

    // ========== Setters ==========

#ifdef TENSOR_BACKEND_LIBTORCH
    /**
     * @brief Add a tensor at a specific time (LibTorch version)
     * @param time The time to add the tensor at
     * @param tensor The tensor to add
     */
    void addTensorAtTime(TimeFrameIndex time, torch::Tensor const & tensor);
    
    /**
     * @brief Overwrite a tensor at a specific time (LibTorch version)
     * @param time The time to overwrite the tensor at
     * @param tensor The tensor to add
     */
    void overwriteTensorAtTime(TimeFrameIndex time, torch::Tensor const & tensor);
#endif

    /**
     * @brief Add a tensor at a specific time (generic version)
     * @param time The time to add the tensor at
     * @param data Raw data to create tensor from
     * @param shape Shape of the tensor
     */
    void addTensorAtTime(TimeFrameIndex time, const std::vector<float>& data, const std::vector<std::size_t>& shape);

    /**
     * @brief Overwrite a tensor at a specific time (generic version)
     * @param time The time to overwrite the tensor at
     * @param data Raw data to create tensor from
     * @param shape Shape of the tensor
     */
    void overwriteTensorAtTime(TimeFrameIndex time, const std::vector<float>& data, const std::vector<std::size_t>& shape);

    // ========== Getters ==========

#ifdef TENSOR_BACKEND_LIBTORCH
    /**
     * @brief Get tensor at a specific time (LibTorch version)
     * @param time The time to get the tensor at
     * @return PyTorch tensor, empty if not found
     */
    [[nodiscard]] torch::Tensor getTensorAtTime(TimeFrameIndex time) const;

    /**
     * @brief Get direct access to internal data (LibTorch version)
     * @return Reference to internal tensor map
     */
    [[nodiscard]] std::map<TimeFrameIndex, torch::Tensor> const & getData() const;
#endif

    /**
     * @brief Get tensor data at a specific time as raw float vector
     * @param time The time to get the tensor at
     * @return Vector of float values, empty if time doesn't exist
     */
    [[nodiscard]] std::vector<float> getTensorDataAtTime(TimeFrameIndex time) const;

    /**
     * @brief Get the shape of tensor at a specific time
     * @param time The time to get the tensor shape at
     * @return Vector of dimensions, empty if time doesn't exist
     */
    [[nodiscard]] std::vector<std::size_t> getTensorShapeAtTime(TimeFrameIndex time) const;

    /**
     * @brief Get all times that have tensors
     * @return Vector of TimeFrameIndex values
     */
    [[nodiscard]] std::vector<TimeFrameIndex> getTimesWithTensors() const;

    /**
     * @brief Get a channel slice from tensor at specific time
     * @param time The time to get the tensor at
     * @param channel The channel to extract
     * @return Vector of float values for the channel slice
     */
    [[nodiscard]] std::vector<float> getChannelSlice(TimeFrameIndex time, int channel) const;

    /**
     * @brief Get the number of tensors stored
     * @return Number of time points with data
     */
    [[nodiscard]] std::size_t size() const;

    /**
     * @brief Get the feature shape (common shape for all tensors)
     * @return Vector of dimensions
     */
    [[nodiscard]] std::vector<std::size_t> getFeatureShape() const;

    /**
     * @brief Set the feature shape
     * @param shape Vector of dimensions
     */
    void setFeatureShape(const std::vector<std::size_t>& shape);

private:
#ifdef TENSOR_BACKEND_LIBTORCH
    std::map<TimeFrameIndex, torch::Tensor> _data;
#else
    std::map<TimeFrameIndex, arma::fcube> _data;
#endif
    std::vector<std::size_t> _feature_shape;

#ifndef TENSOR_BACKEND_LIBTORCH
    // Helper methods for Armadillo backend
    arma::fcube vectorToCube(const std::vector<float>& data, const std::vector<std::size_t>& shape) const;
    std::vector<float> cubeToVector(const arma::fcube& cube) const;
    arma::fmat applySigmoid(const arma::fmat& mat) const;
#else
    // Helper methods for LibTorch backend
    torch::Tensor vectorToTensor(const std::vector<float>& data, const std::vector<std::size_t>& shape) const;
    std::vector<float> tensorToVector(const torch::Tensor& tensor) const;
#endif
};

#endif// TENSOR_DATA_IMPL_HPP
