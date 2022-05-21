# Hazel Audio

Hazel Audio is an audio library which was written for the [Hazel Engine](https://hazelengine.com), built on top of [OpenAL Soft](https://openal-soft.org/). I rewrote Hazel Audio with CMake support and added new features for using in [Oneiro Engine](https://github.com/OneiroGames/Oneiro).

[Original Repo](https://github.com/TheCherno/HazelAudio)

## Currently Supports
- .ogg and .mp3 files
- 3D spatial playback of audio sources
- Control playback
- Unload audio source

## TODO
- Stream audio files
- Audio source seeking
- Listener positioning API
- Wave file support
- Effects

## Example
Check out the `Examples` project for more, but it's super simple:

CMakeLists.txt:
```cmake
cmake_minimum_required(VERSION 3.18)

project(Hazel.AudioExample)

set(CMAKE_CXX_STANDARD 17)

add_executable(Hazel.AudioExample Source/Main.cpp)

add_subdirectory(path/to/hazelaudio/ out/path/)
target_link_libraries(Hazel.AudioExample Hazel.Audio)
```

Main.cpp:
```cpp
// Initialize the audio engine
Hazel::Audio::Init();
// Load audio source from file, bool is for whether the source
// should be in 3D space or not
Hazel::Audio::Source source;
source.LoadFromFile(filename, true);
// Play audio source
source.Play();
```
and you can set various attributes on a source as well:
```cpp
source.SetPosition(x, y, z);
source.SetGain(2.0f);
source.SetLoop(true);
```
and control playback:
```cpp
source.Play();
source.Pause();
source.Stop();
```

## Acknowledgements
- [OpenAL Soft](https://openal-soft.org/)
- [minimp3](https://github.com/lieff/minimp3)
- [libogg and Vorbis](https://www.xiph.org/)

<h1 align="center">Thank you Yan Chernikov for your content❤️</h1>
