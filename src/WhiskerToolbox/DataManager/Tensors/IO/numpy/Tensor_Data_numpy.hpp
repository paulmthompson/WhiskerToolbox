#ifndef WHISKERTOOLBOX_TENSOR_DATA_NUMPY_HPP
#define WHISKERTOOLBOX_TENSOR_DATA_NUMPY_HPP

#include <string>

class TensorData;

void loadNpyToTensorData(std::string const & filepath, TensorData & tensor_data);

#endif//WHISKERTOOLBOX_TENSOR_DATA_NUMPY_HPP
