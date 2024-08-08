
#include "HDF5_Data.hpp"

#include <H5Cpp.h>

#include <algorithm>
#include <iostream>

HDF5Data::HDF5Data() = default;

void HDF5Data::doLoadMedia(std::string name) {

    const int frame_dim = 0;
    const int height_dim = 1;
    const int width_dim = 2;

    auto c_str = name.c_str();
    H5::H5File file(c_str, H5F_ACC_RDONLY);

    for (int i=0; i < file.getNumObjs(); i++) {
        std::cout << file.getObjnameByIdx(i) << std::endl;
    }

    const std::string key = "Data";

    H5::DataSet dataset {file.openDataSet(key)};

    H5::DataSpace dataspace = dataset.getSpace();
    const int n_dims = dataspace.getSimpleExtentNdims();
    std::vector<hsize_t> dims(n_dims);
    dataspace.getSimpleExtentDims(dims.data());

    std::cout << "n_dims: " << dims.size() << '\n';

    std::cout << "shape: (";
    for (hsize_t dim: dims) {
        std::cout << dim << ", ";
    }
    std::cout << ")\n" << std::endl;

    _data = std::vector<uint16_t>(dims[frame_dim] * dims[height_dim] * dims[width_dim]);

    dataset.read(static_cast<void*>(_data.data()), H5::PredType::NATIVE_UINT16);

    updateWidth(dims[width_dim]);
    updateHeight(dims[height_dim]);
    _max_val = *std::max_element(std::begin(_data),std::end(_data));

    std::cout << "Read data" << std::endl;
    std::cout << "Maximum instensity " << _max_val << std::endl;

    file.close();

    setTotalFrameCount(dims[frame_dim]);
}

void HDF5Data::doLoadFrame(int frame_id)
{
    auto start_position = frame_id * getHeight() * getWidth();
    auto frame_data = std::vector<uint8_t>(getHeight() * getWidth());
    for (int i = 0; i < frame_data.size(); i++)
    {
        auto normalized_data = static_cast<float>(_data[start_position + i]) / _max_val;
        frame_data[i] = static_cast<uint8_t>(normalized_data * 256.0f);
    }
    this->setRawData(frame_data);
}

std::string HDF5Data::GetFrameID(int frame_id) {
    return std::to_string(frame_id);
}
