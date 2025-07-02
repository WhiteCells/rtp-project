cmake_minimum_required(VERSION 3.10)

project(rtp-project)


include_directories(/usr/local/include/jrtplib3/)

# link_directories()

link_libraries(/usr/local/lib/libjrtp.a)

set(SRC
    # main.cc
    # main2.cc
    main3.cc
)

add_executable(${PROJECT_NAME} ${SRC})  