#ifndef HDF5_MASK_LOAD_HPP
#define HDF5_MASK_LOAD_HPP

#include <vector>
#include <string>
#include <iostream>

#include <H5Cpp.h>

template <typename T>
std::vector<std::vector<T>> load_hdf5_mask(std::string filepath)
{
    auto c_str = filepath.c_str();
    H5::H5File file(c_str, H5F_ACC_RDONLY);

    for (int i=0; i < file.getNumObjs(); i++) {
        std::cout << file.getObjnameByIdx(i) << std::endl;
    }


    file.close();

    return std::vector<std::vector<float>>{};
}



#endif // HDF5_MASK_LOAD_HPP
