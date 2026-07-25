cmake_minimum_required(VERSION 3.10)
project(GDIEffect)

set(CMAKE_CXX_STANDARD 17)

add_executable(GDIEffect main.cpp)

target_link_libraries(GDIEffect winmm gdi32 user32)

if(MINGW)
    target_link_options(GDIEffect PRIVATE -static)
endif()