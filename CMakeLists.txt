cmake_minimum_required(VERSION 3.18)

project(Hazel.Audio)

set(CMAKE_CXX_STANDARD 17)
set(ALSOFT_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ALSOFT_UTILS OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_CONFIG OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_HRTF_DATA OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_AMBDEC_PRESETS OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_UTILS OFF CACHE BOOL "" FORCE)
set(ALSOFT_UPDATE_BUILD_VERSION OFF CACHE BOOL "" FORCE)
set(INSTALL_CMAKE_PACKAGE_MODULE OFF CACHE BOOL "" FORCE)

find_package(Threads REQUIRED)

include_directories(Include/)

add_library(Hazel.Audio Source/alhelpers.cpp Source/HazelAudio.cpp)

add_subdirectory(ThirdParty/openal ThirdParty/openal)
add_subdirectory(ThirdParty/vorbis ThirdParty/vorbis)
target_link_libraries(Hazel.Audio PUBLIC OpenAL Vorbis::vorbis Vorbis::vorbisfile Threads::Threads)
target_include_directories(Hazel.Audio PUBLIC Include/ ThirdParty/minimp3)