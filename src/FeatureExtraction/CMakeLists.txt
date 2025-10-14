add_library(feature_extraction STATIC
    IFeature.hpp
    IFeature.cpp
    FeatureVector.hpp
    FeatureExtractor.hpp
)

target_include_directories(feature_extraction PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_link_libraries(feature_extraction PUBLIC NEURALYZER_GEOMETRY)


if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(feature_extraction PRIVATE ${CLANG_OPTIONS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(feature_extraction PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(feature_extraction PRIVATE ${MSVC_WARNINGS})
endif()

set_target_properties(feature_extraction PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
