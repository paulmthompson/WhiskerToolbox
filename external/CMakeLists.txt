

include(FetchContent)

FetchContent_Declare(
  ffmpeg_wrapper
  GIT_REPOSITORY https://github.com/paulmthompson/ffmpeg_wrapper.git
  GIT_TAG 45133f0a4cbc68d1f12c30ff3d9b229b75b48d78
)

#add_subdirectory(ffmpeg_wrapper)

FetchContent_Declare(
  Whisker-Analysis
  GIT_REPOSITORY https://github.com/paulmthompson/Whisker-Analysis.git
  GIT_TAG c70c20f9421fb7fc603ca62b61df4993a3f22b5e
  CMAKE_ARGS -DBUILD_WHISKERTRACKER=OFF
)

#add_subdirectory(Whisker-Analysis)

#We should use make avaiable
FetchContent_MakeAvailable(ffmpeg_wrapper Whisker-Analysis)

set_target_properties(ffmpeg_wrapper PROPERTIES EXCLUDE_FROM_ALL TRUE)
set_target_properties(Whisker-Analysis PROPERTIES EXCLUDE_FROM_ALL TRUE)

