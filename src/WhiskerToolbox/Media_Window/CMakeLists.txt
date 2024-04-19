
add_library(Media_Window SHARED
        Media_Window.h
        Media_Window.cpp

        Video_Window.h
        Video_Window.cpp

        Images_Window.h
        Images_Window.cpp
)

target_include_directories(Media_Window PUBLIC
                            "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>")

target_link_libraries(Media_Window PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_link_libraries(Media_Window PRIVATE Qt${QT_VERSION_MAJOR}::Multimedia)
target_link_libraries(Media_Window PRIVATE Qt${QT_VERSION_MAJOR}::MultimediaWidgets)
target_link_libraries(Media_Window PRIVATE Qt${QT_VERSION_MAJOR}::OpenGL)
target_link_libraries(Media_Window PRIVATE Qt${QT_VERSION_MAJOR}::UiTools)

target_link_libraries(Media_Window PRIVATE DataManager)
target_link_libraries(Media_Window PRIVATE ffmpeg_wrapper::ffmpeg_wrapper)
