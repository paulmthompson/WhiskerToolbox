
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include(FetchContent)
set(BUILD_TESTING OFF)
FetchContent_Declare(
  ffmpeg_wrapper
  GIT_REPOSITORY https://github.com/paulmthompson/ffmpeg_wrapper.git
  GIT_TAG 807db81584dbf72d34db3659ac01f286bcf7fe53
)

#add_subdirectory(ffmpeg_wrapper)

FetchContent_Declare(
  Whisker-Analysis
  GIT_REPOSITORY https://github.com/paulmthompson/Whisker-Analysis.git
  GIT_TAG c70c20f9421fb7fc603ca62b61df4993a3f22b5e
  CMAKE_ARGS -DBUILD_WHISKERTRACKER=OFF -BUILD_TESTING=OFF

)

FetchContent_Declare(
  qt6advanceddocking
  GIT_REPOSITORY https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System
  GIT_TAG 4.3.1
  CMAKE_ARGS -DBUILD_EXAMPLES=OFF
)

Set(FETCHCONTENT_QUIET FALSE)
FetchContent_Declare(JKQTPlotter6
                     GIT_REPOSITORY https://github.com/jkriege2/JKQtPlotter.git
                     GIT_TAG aa4ac4c58a099594e88e3c63110dff69136d54b8
                     CMAKE_ARGS -DJKQtPlotter_BUILD_EXAMPLES=OFF -DJKQtPlotter_BUILD_TESTS=OFF -DJKQtPlotter_BUILD_TOOLS=OFF -DJKQtPlotter_BUILD_LIB_JKQTFASTPLOTTER=OFF
                     )
# ... finally make JKQTPlotter5/6 available

#add_subdirectory(Whisker-Analysis)

#We should use make avaiable
FetchContent_MakeAvailable(ffmpeg_wrapper Whisker-Analysis qt6advanceddocking JKQTPlotter6)

set_target_properties(ffmpeg_wrapper PROPERTIES EXCLUDE_FROM_ALL TRUE)
set_target_properties(Whisker-Analysis PROPERTIES EXCLUDE_FROM_ALL TRUE)
set_target_properties(qt6advanceddocking PROPERTIES
        EXCLUDE_FROM_ALL TRUE
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
