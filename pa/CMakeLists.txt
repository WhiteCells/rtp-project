cmake_minimum_required(VERSION 3.10)

project(portaudio_server LANGUAGES CXX VERSION 1.0)

set(SRC
    main.cc
)

add_executable(${PROJECT_NAME} ${SRC})

target_link_libraries(${PROJECT_NAME} PRIVATE
    portaudio
)
