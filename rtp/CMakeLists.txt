cmake_minimum_required(VERSION 3.10)

project(rtp VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 如果 libjrtp 和 portaudio 安装在 /usr/local/lib 和 /usr/local/include
# 可以添加这两个目录（如不需要可注释）
include_directories(/usr/local/include)
link_directories(/usr/local/lib)

# 添加可执行文件
add_executable(sender sender.cc)
add_executable(receiver receiver.cc)
add_executable(test test.cc)
add_executable(pipe pipe.cc)

# 链接所需库
target_link_libraries(sender jrtp portaudio pthread)
target_link_libraries(receiver jrtp portaudio pthread)
target_link_libraries(test jrtp portaudio pthread)
target_link_libraries(pipe jrtp portaudio pthread)
