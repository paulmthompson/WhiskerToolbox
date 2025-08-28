# Create MediaDisplay static library
add_library(MediaDisplay STATIC
    MediaDisplayManager.cpp
    MediaDisplayManager.hpp
    MediaDisplayCoordinator.cpp
    MediaDisplayCoordinator.hpp
    EnhancedMediaExport_Widget.cpp
    EnhancedMediaExport_Widget.hpp
)

# Set target properties
set_target_properties(MediaDisplay PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
    AUTOMOC ON
)

# Link Qt6 libraries
target_link_libraries(MediaDisplay PUBLIC Qt6::Widgets Qt6::Core Qt6::Gui)

# Link to DataManager (which Media_Widget and Media_Window already depend on)
target_link_libraries(MediaDisplay PUBLIC DataManager)

# Include directories - we need access to Media_Widget and Media_Window headers
target_include_directories(MediaDisplay PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)
