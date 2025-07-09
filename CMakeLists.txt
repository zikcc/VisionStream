cmake_minimum_required(VERSION 3.10.0)
project(VisionStream VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)  # 确保 Debug 模式

# —— 找到 libv4l2 —— 
find_path(V4L2_INCLUDE_DIR
  NAMES linux/videodev2.h
  HINTS /usr/include /usr/local/include
)
find_library(V4L2_LIBRARY
  NAMES v4l2
  HINTS /usr/lib /usr/local/lib
)
# 查找 SDL2 库
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})
# FFmpeg
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED 
    libavcodec 
    libavformat 
    libavutil 
    libswscale 
    libavdevice
)
# OpenCV
set(OpenCV_DIR "/usr/local/lib/cmake/opencv4") 
# set(OpenCV_DIR "/usr/local/include/opencv4") 
find_package(OpenCV REQUIRED)
message(STATUS "Found OpenCV ${OpenCV_VERSION}")
# 线程库
find_package(Threads REQUIRED)


# —— vision 库 —— 
add_library(vision
    src/capture/V4L2Capture.cpp
    src/processor/OpenCVProcessor.cpp
)

# 导出头文件位置
target_include_directories(vision
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_include_directories(vision
    PUBLIC 
      ${V4L2_INCLUDE_DIR}
)
target_link_libraries(vision
    PUBLIC
      ${V4L2_LIBRARY}
      ${OpenCV_LIBS}
      Threads::Threads
)


# ----- local_display -----
add_executable(local_display
    app/LocalDisplayApp.cpp   
)
target_link_libraries(local_display PRIVATE
    ${SDL2_LIBRARIES} 
    vision
)
# ----- push_stream -----
add_executable(push_stream
    app/PushStreamApp.cpp   
    src/streamer/RTMPStreamer.cpp   
)
target_link_libraries(push_stream PRIVATE
    ${FFMPEG_LIBRARIES}
    vision
)
target_include_directories(push_stream PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
