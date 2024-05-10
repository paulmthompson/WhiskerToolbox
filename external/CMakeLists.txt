

include(FetchContent)

FetchContent_Declare(
  ffmpeg_wrapper
  GIT_REPOSITORY https://github.com/paulmthompson/ffmpeg_wrapper.git
  GIT_TAG 5af3fc845733a29e20220d4cc6e680fb6f0d975f
)

add_subdirectory(ffmpeg_wrapper)
