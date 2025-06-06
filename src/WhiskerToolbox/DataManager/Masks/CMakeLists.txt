
set(mask_sources
    masks.hpp
    masks.cpp
    Mask_Data.hpp
    Mask_Data.cpp
    IO/HDF5/Mask_Data_HDF5.hpp
    IO/HDF5/Mask_Data_HDF5.cpp
    IO/Image/Mask_Data_Image.hpp
    IO/Image/Mask_Data_Image.cpp
    IO/JSON/Mask_Data_JSON.hpp
    IO/JSON/Mask_Data_JSON.cpp
    utils/connected_component.hpp
    utils/connected_component.cpp
    utils/skeletonize.cpp
    utils/skeletonize.hpp)

set(mask_test_sources
    masks.test.cpp
    Mask_Data.test.cpp
    utils/connected_component.test.cpp
    utils/skeletonize.test.cpp)

prefix_list_items(mask_sources "Masks")
set(DATAMANAGER_SOURCES ${DATAMANAGER_SOURCES} ${mask_sources} PARENT_SCOPE)

add_tests_to_global(mask_test_sources
                    "${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager/Masks"
                    DATAMANAGER_TEST_SOURCES)
