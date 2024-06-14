

include(FetchContent)

FetchContent_Declare(
  ffmpeg_wrapper
  GIT_REPOSITORY https://github.com/paulmthompson/ffmpeg_wrapper.git
  GIT_TAG 60343384c3884e0d910a8bc9a08782175fe24b38
)

add_subdirectory(ffmpeg_wrapper)

FetchContent_Declare(
  Whisker-Analysis
  GIT_REPOSITORY https://github.com/paulmthompson/Whisker-Analysis.git
  GIT_TAG 502040d7b3d846d4906fd278062cfc0f983b0e3c
)

add_subdirectory(Whisker-Analysis)

#We should use make avaiable
FetchContent_MakeAvailable(ffmpeg_wrapper Whisker-Analysis)
