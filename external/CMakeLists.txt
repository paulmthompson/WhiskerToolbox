

include(FetchContent)

FetchContent_Declare(
  ffmpeg_wrapper
  GIT_REPOSITORY https://github.com/paulmthompson/ffmpeg_wrapper.git
  GIT_TAG 0b95db6af89a411994910acfbccc36d87d8212e9
)

add_subdirectory(ffmpeg_wrapper)
