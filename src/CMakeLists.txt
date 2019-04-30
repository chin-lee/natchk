set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME ON)
find_package(Boost REQUIRED COMPONENTS system date_time chrono regex)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(${PROJECT_ROOT_DIR}/src)

# natchk library
set(SRCS 
    longopt.cpp 
    longopt.h)
source_group("" FILES ${SRCS})

add_library(natchk ${SRCS})
target_link_libraries(natchk)

# server
set(SERVER_SRCS
    server/main.cpp)
source_group("" FILES ${SERVER_SRCS})
    
add_executable(natchk-svr ${SERVER_SRCS})
target_link_libraries(natchk-svr 
    natchk 
    ${Boost_LIBRARIES})
    
# client
set(CLIENT_SRCS
    client/main.cpp)
source_group("" FILES ${CLIENT_SRCS})

add_executable(natchk-cli ${CLIENT_SRCS})
target_link_libraries(natchk-cli 
    natchk 
    ${Boost_LIBRARIES})