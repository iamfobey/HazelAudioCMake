cmake_minimum_required(VERSION 3.18)

project(Hazel.Audio)

set(CMAKE_CXX_STANDARD 17)
set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Threads REQUIRED)

include_directories(Include/)

add_library(Hazel.Audio Source/alhelpers.cpp Source/HazelAudio.cpp)

add_subdirectory(ThirdParty/openal ThirdParty/openal)
add_subdirectory(ThirdParty/vorbis ThirdParty/vorbis)
target_link_libraries(Hazel.Audio PUBLIC OpenAL Vorbis::vorbis Vorbis::vorbisfile Threads::Threads pthread)
target_include_directories(Hazel.Audio PUBLIC Include/ ThirdParty/minimp3)