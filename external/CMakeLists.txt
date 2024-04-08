

include(FetchContent)

FetchContent_Declare(
  ffmpeg_wrapper
  GIT_REPOSITORY https://github.com/paulmthompson/ffmpeg_wrapper.git
  GIT_TAG 20e2bc760cfeedb4b645865fa450ca25fdc1c968
)

add_subdirectory(ffmpeg_wrapper)
