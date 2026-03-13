#ifndef WHISKERTOOLBOX_TENSOR_DATA_NUMPY_HPP
#define WHISKERTOOLBOX_TENSOR_DATA_NUMPY_HPP

#include <string>

class TensorData;

/**
 * @brief Load numpy data into TensorData
 * 
 * Loads a .npy file where the first dimension is treated as time and remaining
 * dimensions are flattened into features. Creates a default TimeFrame with
 * sequential time values [0.0, 1.0, 2.0, ...].
 * 
 * @param filepath Path to the .npy file
 * @param tensor_data TensorData instance to populate (will be replaced with new data)
 */
void loadNpyToTensorData(std::string const & filepath, TensorData & tensor_data);

#endif//WHISKERTOOLBOX_TENSOR_DATA_NUMPY_HPP
