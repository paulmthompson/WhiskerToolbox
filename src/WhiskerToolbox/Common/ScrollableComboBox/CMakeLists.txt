
find_package(Qt6 REQUIRED COMPONENTS Widgets Core)

set(SCROLLABLE_COMBO_BOX_SOURCES
    ScrollableComboBox.cpp
    ScrollableComboBox.hpp
)

add_library(ScrollableComboBox STATIC ${SCROLLABLE_COMBO_BOX_SOURCES})

set_target_properties(ScrollableComboBox PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(ScrollableComboBox PUBLIC Qt6::Widgets Qt6::Core)

target_include_directories(ScrollableComboBox PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

set_target_compiler_warnings(ScrollableComboBox)

add_library(WhiskerToolbox::ScrollableComboBox ALIAS ScrollableComboBox)
