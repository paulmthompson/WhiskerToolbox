

include(FetchContent)

FetchContent_Declare(
  ffmpeg_wrapper
  GIT_REPOSITORY https://github.com/paulmthompson/ffmpeg_wrapper.git
  GIT_TAG 0d206b2b71d3587fb535b69e098db63ba09bbdb4
)

add_subdirectory(ffmpeg_wrapper)
