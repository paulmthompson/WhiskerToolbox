find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(LAYOUT_TESTING_SOURCES
        LayoutViolation.hpp
        LayoutSanityChecker.hpp
        LayoutSanityChecker.cpp
)

qt_add_library(LayoutTesting STATIC ${LAYOUT_TESTING_SOURCES})

set_target_properties(LayoutTesting PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(LayoutTesting PUBLIC Qt6::Widgets)

target_include_directories(LayoutTesting PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(LayoutTesting PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(LayoutTesting PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(LayoutTesting PRIVATE ${MSVC_WARNINGS})
endif()

set_target_properties(LayoutTesting PROPERTIES
    AUTOMOC ON
)
enable_whiskertoolbox_pch(LayoutTesting)