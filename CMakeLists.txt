cmake_minimum_required(VERSION 3.0)
project("natchk" CXX)

set(CMAKE_CXX_STANDARD 14)

# libuv 
add_subdirectory(libuv)
include_directories(libuv/include)

# natchk.lib
set(SRCS
    longopt.cpp 
    longopt.h
    async.cpp
    async.h
    endpoint.cpp
    endpoint.h
    util.h
    log.h)
    
source_group("" FILES ${SRCS})
add_library(natchk ${SRCS})

# natchk-cli
set(CLIENT_SRCS client.cpp)
source_group("" FILES ${CLIENT_SRCS})
add_executable(natchk-cli ${CLIENT_SRCS})
target_link_libraries(natchk-cli natchk uv_a)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(natchk-cli pthread)
endif()

# natchk-svr
set(SERVER_SRCS server.cpp)
source_group("" FILES ${SERVER_SRCS})
add_executable(natchk-svr ${SERVER_SRCS})
target_link_libraries(natchk-svr natchk uv_a)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(natchk-cli pthread)
endif()