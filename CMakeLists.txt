cmake_minimum_required(VERSION 3.16)
enable_language(CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "-g")

find_package(Boost 1.72.0 REQUIRED COMPONENTS thread iostreams filesystem)

include_directories(include)

if(WIN32)
set(libs
    wsock32
    ws2_32)
endif()

project(cli)
set(cli_src client/src/main.cpp)
add_executable(cli ${cli_src})
target_link_libraries(cli
    PRIVATE
        ${Boost_LIBRARIES}
        ${libs})

project(serv)
set(serv_src server/src/main.cpp)
add_executable(serv ${serv_src})
target_link_libraries(serv
    PRIVATE
        ${Boost_LIBRARIES}
        ${libs})
