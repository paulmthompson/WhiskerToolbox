

include(FetchContent)

FetchContent_Declare(
  ffmpeg_wrapper
  GIT_REPOSITORY https://github.com/paulmthompson/ffmpeg_wrapper.git
  GIT_TAG 0ab33b1bc9fef636e0bd154876d65d98dc9ec2eb
)

add_subdirectory(ffmpeg_wrapper)

FetchContent_Declare(
  Whisker-Analysis
  GIT_REPOSITORY https://github.com/paulmthompson/Whisker-Analysis.git
  GIT_TAG 626f0917f8f15384b0c8875d9e2792de896882df
)

add_subdirectory(Whisker-Analysis)

#We should use make avaiable
FetchContent_MakeAvailable(ffmpeg_wrapper Whisker-Analysis)
