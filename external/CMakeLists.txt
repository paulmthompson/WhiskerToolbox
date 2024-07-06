

include(FetchContent)

FetchContent_Declare(
  ffmpeg_wrapper
  GIT_REPOSITORY https://github.com/paulmthompson/ffmpeg_wrapper.git
  GIT_TAG e9a7211bcdbee8e3456b6b9f9483c6331de44a00
)

#add_subdirectory(ffmpeg_wrapper)

FetchContent_Declare(
  Whisker-Analysis
  GIT_REPOSITORY https://github.com/paulmthompson/Whisker-Analysis.git
  GIT_TAG c8de94f71984620f2029d3a1de8c1c57f23ae408
)

#add_subdirectory(Whisker-Analysis)

#We should use make avaiable
FetchContent_MakeAvailable(ffmpeg_wrapper Whisker-Analysis)



