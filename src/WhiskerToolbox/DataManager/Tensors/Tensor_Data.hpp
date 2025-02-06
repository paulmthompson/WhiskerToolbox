#ifndef TENSOR_DATA_HPP
#define TENSOR_DATA_HPP

#include "Observer/Observer_Data.hpp"

#include <torch/torch.h>

#include <map>
#include <vector>

class TensorData : public ObserverData {
public:
    TensorData() = default;

    template <typename T>
    TensorData(std::map<int, torch::Tensor> data, std::vector<T> shape)
        : _data(std::move(data))
    {
        for (auto s : shape){
            _feature_shape.push_back(static_cast<std::size_t>(s));
        }
    }

    void addTensorAtTime(int time, const torch::Tensor& tensor);
    void overwriteTensorAtTime(int time, const torch::Tensor& tensor);
    torch::Tensor getTensorAtTime(int time) const;
    std::vector<int> getTimesWithTensors() const;
    std::vector<float> getChannelSlice(int time, int channel);

    const std::map<int, torch::Tensor>& getData() const { return _data; }

    std::size_t size() const {return _data.size();}

    std::vector<std::size_t> getFeatureShape() const {
        return _feature_shape;
    }

private:
    std::map<int, torch::Tensor> _data;
    std::vector<size_t> _feature_shape;
};

void loadNpyToTensorData(const std::string& filepath, TensorData& tensor_data);

#endif // TENSOR_DATA_HPP
