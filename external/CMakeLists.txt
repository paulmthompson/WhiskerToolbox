cmake_minimum_required(VERSION 3.21)

include(FetchContent)

FetchContent_Declare(
  ffmpeg_wrapper
  GIT_REPOSITORY https://github.com/paulmthompson/ffmpeg_wrapper.git
  GIT_TAG efa1de080ad18d70f196d1e95e3fe279eb87ed36
)

add_subdirectory(ffmpeg_wrapper)
