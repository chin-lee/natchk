cmake_minimum_required(VERSION 3.0)
project("natchk" CXX)

if(MSVC)
    add_compile_options(
        "/W4" "$<$<CONFIG:RELEASE>:/O2>"
        "-D_WIN32_WINNT=0x0501"
        "-D_CRT_SECURE_NO_WARNINGS")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
else()
    add_compile_options("-g" "-Wall" "-Wextra" "$<$<CONFIG:RELEASE>:-O2>")
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        add_compile_options("-stdlib=libc++")
    endif()
endif()

set(CMAKE_CXX_STANDARD 14)
set(PROJECT_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(src)