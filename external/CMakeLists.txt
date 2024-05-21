

include(FetchContent)

FetchContent_Declare(
  ffmpeg_wrapper
  GIT_REPOSITORY https://github.com/paulmthompson/ffmpeg_wrapper.git
  GIT_TAG 226303bc2eee94a962a7b5d21c8c4125b0231845
)

add_subdirectory(ffmpeg_wrapper)
