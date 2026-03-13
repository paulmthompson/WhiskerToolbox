add_subdirectory(TypeTraits)
add_subdirectory(DataTypeEnum)

add_subdirectory(AnalogTimeSeries)
add_subdirectory(DigitalTimeSeries)

add_subdirectory(RaggedTimeSeries)

add_subdirectory(Lines)
add_subdirectory(Points)
add_subdirectory(Masks)

add_subdirectory(Media)

if (ENABLE_OPENCV)
# OPEN CV INTERFERES WITH TENSOR DATA. we need to import it first
    find_package(OpenCV CONFIG REQUIRED) 
endif()

add_subdirectory(Tensors)

