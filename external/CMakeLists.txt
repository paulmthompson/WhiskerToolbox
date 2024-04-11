

include(FetchContent)

FetchContent_Declare(
  ffmpeg_wrapper
  GIT_REPOSITORY https://github.com/paulmthompson/ffmpeg_wrapper.git
  GIT_TAG c6ce21f847647788748fe64ad3f89534a649bc7d
)

add_subdirectory(ffmpeg_wrapper)
