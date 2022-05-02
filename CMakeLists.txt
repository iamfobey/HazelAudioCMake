cmake_minimum_required(VERSION 3.18)

project(Hazel.Audio)

set(CMAKE_CXX_STANDARD 17)

include_directories(Include/)

add_library(Hazel.Audio Source/alhelpers.cpp Source/HazelAudio.cpp)

add_subdirectory(ThirdParty/openal-soft ThirdParty/openal-soft)
add_subdirectory(ThirdParty/vorbis ThirdParty/vorbis)
target_link_libraries(Hazel.Audio PRIVATE OpenAL Vorbis::vorbis Vorbis::vorbisfile)
target_include_directories(Hazel.Audio PUBLIC Include/ ThirdParty/minimp3)