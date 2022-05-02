cmake_minimum_required(VERSION 3.18)

project(Hazel.AudioExample)

set(CMAKE_CXX_STANDARD 17)

add_executable(Hazel.AudioExample Source/HazelAL-Example.cpp)

add_subdirectory(../ HazelAudio)
target_link_libraries(Hazel.AudioExample Hazel.Audio)