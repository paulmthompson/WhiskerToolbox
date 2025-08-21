set(MEDIA_EXPORT_SOURCES
    media_export.cpp
    media_export.hpp
    MediaExport_Widget.cpp
    MediaExport_Widget.hpp
    MediaExport_Widget.ui
)

# Create the static library
add_library(MediaExport STATIC ${MEDIA_EXPORT_SOURCES})

# Set target properties
set_target_properties(MediaExport PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

# Find Qt6 components
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

# Link Qt6 libraries
target_link_libraries(MediaExport PRIVATE Qt6::Widgets Qt6::Core Qt6::Gui)

# Link DataManager
target_link_libraries(MediaExport PRIVATE DataManager)

# Set include directories
target_include_directories(MediaExport PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" #For DataManager
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Enable Qt MOC
set_target_properties(MediaExport PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
)

# Set C++ standard
target_compile_features(MediaExport PUBLIC cxx_std_20)

# Add compiler-specific options
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(MediaExport PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(MediaExport PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(MediaExport PRIVATE ${MSVC_WARNINGS})
endif()

# Install rules
install(TARGETS MediaExport
    EXPORT MediaExportTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES 
    media_export.hpp
    MediaExport_Widget.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/MediaExport
)

install(FILES 
    MediaExport_Widget.ui
    DESTINATION ${CMAKE_INSTALL_DATADIR}/MediaExport
)
