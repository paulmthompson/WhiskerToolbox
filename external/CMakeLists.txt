

include(FetchContent)

FetchContent_Declare(
  ffmpeg_wrapper
  GIT_REPOSITORY https://github.com/paulmthompson/ffmpeg_wrapper.git
  GIT_TAG 8c496f7cf44b2be4631cf66dd5b28f2ffb3909bb
)

add_subdirectory(ffmpeg_wrapper)

FetchContent_Declare(
  Whisker-Analysis
  GIT_REPOSITORY https://github.com/paulmthompson/Whisker-Analysis.git
  GIT_TAG 1ff57c6c6b547117a82333f69ea0b1d6b43265c4
)

add_subdirectory(Whisker-Analysis)

#We should use make avaiable
FetchContent_MakeAvailable(ffmpeg_wrapper Whisker-Analysis)
