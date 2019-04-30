cmake_minimum_required(VERSION 3.0)
project("natchk" CXX)

set(CMAKE_CXX_STANDARD 14)

set(CLIENT_SRCS 
    client.cpp
    async.cpp
    async.h
    endpoint.cpp
    endpoint.h)

set(LONGOPT_SRCS 
    longopt.cpp 
    longopt.h)

add_executable(natchk-cli 
    ${CLIENT_SRCS} 
    ${LONGOPT_SRCS})

target_link_libraries(natchk-cli libuv.a pthread)

set(SERVER_SRCS
    server.cpp
    async.cpp
    async.h
    endpoint.cpp
    endpoint.h)

add_executable(natchk-svr
    ${SERVER_SRCS}
    ${LONGOPT_SRCS})

target_link_libraries(natchk-svr libuv.a pthread)