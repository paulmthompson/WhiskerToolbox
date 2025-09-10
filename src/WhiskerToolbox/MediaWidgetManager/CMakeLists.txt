set(MEDIA_WIDGET_MANAGER_SOURCES
    MediaWidgetManager.cpp
)

set(MEDIA_WIDGET_MANAGER_HEADERS
    MediaWidgetManager.hpp
)

add_library(MediaWidgetManager 
    ${MEDIA_WIDGET_MANAGER_SOURCES}
    ${MEDIA_WIDGET_MANAGER_HEADERS}
)

target_link_libraries(MediaWidgetManager
    PUBLIC
        Qt6::Core
        Qt6::Widgets
        DataManager
        Media_Widget
)

target_include_directories(MediaWidgetManager
    PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>" #Media_Widget
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" #DataManager
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(MediaWidgetManager PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(MediaWidgetManager PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(MediaWidgetManager PRIVATE ${MSVC_WARNINGS})
endif()