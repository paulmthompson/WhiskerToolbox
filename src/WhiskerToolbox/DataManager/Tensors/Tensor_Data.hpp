#ifndef TENSOR_DATA_HPP
#define TENSOR_DATA_HPP

#include "Observer/Observer_Data.hpp"
#include <torch/torch.h>
#include <map>

class TensorData : public ObserverData {
public:
    TensorData() = default;
    TensorData(std::map<int, torch::Tensor> data) : _data(std::move(data)) {}

    void addTensorAtTime(int time, const torch::Tensor& tensor);
    void overwriteTensorAtTime(int time, const torch::Tensor& tensor);
    torch::Tensor getTensorAtTime(int time) const;
    std::vector<int> getTimesWithTensors() const;

    const std::map<int, torch::Tensor>& getData() const { return _data; }

    std::size_t size() const {return _data.size();}

private:
    std::map<int, torch::Tensor> _data;
};

void loadNpyToTensorData(const std::string& filepath, TensorData& tensor_data);

#endif // TENSOR_DATA_HPP
