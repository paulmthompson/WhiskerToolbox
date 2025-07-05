#ifndef TENSOR_DATA_HPP
#define TENSOR_DATA_HPP

#include "Observer/Observer_Data.hpp"
#include "TimeFrame.hpp"

#include <torch/torch.h>

#include <map>
#include <vector>

class TensorData : public ObserverData {
public:

    // ========== Constructors ==========
    TensorData() = default;

    template<typename T>
    TensorData(std::map<TimeFrameIndex, torch::Tensor> data, std::vector<T> shape)
        : _data(std::move(data)) {
        for (auto s: shape) {
            _feature_shape.push_back(static_cast<std::size_t>(s));
        }
    }

    // ========== Setters ==========

    void addTensorAtTime(TimeFrameIndex time, torch::Tensor const & tensor);


    void overwriteTensorAtTime(TimeFrameIndex time, torch::Tensor const & tensor);

    // ========== Getters ==========

    [[nodiscard]] torch::Tensor getTensorAtTime(TimeFrameIndex time) const;
    [[nodiscard]] std::vector<TimeFrameIndex> getTimesWithTensors() const;
    [[nodiscard]] std::vector<float> getChannelSlice(TimeFrameIndex time, int channel) const;

    [[nodiscard]] std::map<TimeFrameIndex, torch::Tensor> const & getData() const { return _data; }

    [[nodiscard]] std::size_t size() const { return _data.size(); }

    [[nodiscard]] std::vector<std::size_t> getFeatureShape() const {
        return _feature_shape;
    }

private:
    std::map<TimeFrameIndex, torch::Tensor> _data;
    std::vector<size_t> _feature_shape;
};

void loadNpyToTensorData(std::string const & filepath, TensorData & tensor_data);

#endif// TENSOR_DATA_HPP
