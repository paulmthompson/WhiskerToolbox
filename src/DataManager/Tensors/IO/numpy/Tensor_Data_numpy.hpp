#ifndef WHISKERTOOLBOX_TENSOR_DATA_NUMPY_HPP
#define WHISKERTOOLBOX_TENSOR_DATA_NUMPY_HPP

#include <string>

class TensorData;

/**
 * @brief Load numpy data into TensorData
 * @param filepath Path to the .npy file
 * @param tensor_data TensorData instance to load into
 */
void loadNpyToTensorData(std::string const & filepath, TensorData & tensor_data);

#endif//WHISKERTOOLBOX_TENSOR_DATA_NUMPY_HPP
