cmake_minimum_required(VERSION 3.21)

include(FetchContent)

FetchContent_Declare(
  ffmpeg_wrapper
  GIT_REPOSITORY https://github.com/paulmthompson/ffmpeg_wrapper.git
  GIT_TAG 3ae5e4640d791a6e4a445068560d7e4e3b37a308
)

add_subdirectory(ffmpeg_wrapper)
