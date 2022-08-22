#include "HazelAudio/HazelAudio.h"

#include <filesystem>
#include <string>
#include <thread>

#include "al.h"
#include "alc.h"
#include "alext.h"
#include "alhelpers.h"

#define MINIMP3_IMPLEMENTATION
#include <cassert>

#include "minimp3.h"
#include "minimp3_ex.h"

#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"

#include <vector>
#include <future>

namespace Hazel::Audio
{
    static ALCdevice* s_AudioDevice{};
    static mp3dec_t s_Mp3d{};
    static std::vector<std::pair<const std::string, Hazel::Audio::Source*>> s_AudioSources{};

    // Currently supported file formats
    enum class AudioFileFormat : uint8_t
    {
        None = 0,
        Ogg,
        MP3
    };

    static AudioFileFormat GetFileFormat(const std::string& filename)
    {
        const std::filesystem::path path = filename;
        const std::string extension = path.extension().string();

        if (extension == ".ogg")
            return AudioFileFormat::Ogg;
        if (extension == ".mp3")
            return AudioFileFormat::MP3;

        return AudioFileFormat::None;
    }

    static ALenum GetOpenALFormat(uint32_t channels)
    {
        // Note: sample size is always 2 bytes (16-bits) with
        // both the .mp3 and .ogg decoders that we're using
        switch (channels)
        {
        case 1: return AL_FORMAT_MONO16;
        case 2: return AL_FORMAT_STEREO16;
        default: assert(false);
        }
    }

    bool PreInit()
    {
        if (InitAL(s_AudioDevice, nullptr, nullptr) != 0)
            return false;

        mp3dec_init(&s_Mp3d);

        // Init listener
        constexpr ALfloat listenerPos[] = {0.0f};
        constexpr ALfloat listenerVel[] = {0.0f};
        constexpr ALfloat listenerOri[] = {0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f};
        alListenerfv(AL_POSITION, listenerPos);
        alListenerfv(AL_VELOCITY, listenerVel);
        alListenerfv(AL_ORIENTATION, listenerOri);

        return true;
    }

    bool Init()
    {
        std::vector<std::future<void>> futures{};
        for (auto& audioSource : s_AudioSources)
        {
            futures.emplace_back(std::async(std::launch::async, [&](){
                switch (GetFileFormat(audioSource.first))
                {
                case AudioFileFormat::Ogg:
                {
                    FILE* f = fopen(audioSource.first.c_str(), "rb");

                    OggVorbis_File vf;
                    if (ov_open_callbacks(f, &vf, nullptr, 0, OV_CALLBACKS_NOCLOSE) < 0) {
                        throw std::runtime_error("Ogg Error in " + audioSource.first + " file!");
                    }

                    const vorbis_info* vi = ov_info(&vf, -1);
                    audioSource.second->mSampleRate = vi->rate;
                    audioSource.second->mChannels = vi->channels;
                    audioSource.second->mAlFormat = GetOpenALFormat(audioSource.second->mChannels);
                    const auto samples = ov_pcm_total(&vf, -1);
                    const auto bufferSize = 2 * audioSource.second->mChannels * samples; // 2 bytes per sample (I'm guessing...)

                    audioSource.second->mTotalDuration = static_cast<float>(samples) / static_cast<float>(audioSource.second->mSampleRate); // in seconds
                    audioSource.second->mLoaded = true;
                    audioSource.second->mOggBuffer = new uint8_t[bufferSize];

                    auto* bufferPtr = audioSource.second->mOggBuffer;
                    while (true)
                    {
                        int currentSection{};
                        const auto length = ov_read(&vf, reinterpret_cast<char*>(bufferPtr), 4096, 0, 2, 1, &currentSection);
                        if (length == 0)
                            break;
                        else if (length < 0)
                        {
                            if (length == OV_EBADLINK)
                            {
                                fprintf(stderr, "Corrupt bitstream section! Exiting.\n");
                                exit(1);
                            }
                        }
                        bufferPtr += length;
                    }

                    audioSource.second->mSize = bufferPtr - audioSource.second->mOggBuffer;
                    assert(bufferSize == audioSource.second->mSize);

                    alGenBuffers(1, &audioSource.second->mBufferHandle);
                    alBufferData(audioSource.second->mBufferHandle, audioSource.second->mAlFormat, audioSource.second->mOggBuffer,
                                 audioSource.second->mSize, audioSource.second->mSampleRate);
                    alGenSources(1, &audioSource.second->mSourceHandle);
                    alSourcei(audioSource.second->mSourceHandle, AL_BUFFER, audioSource.second->mBufferHandle);

                    delete[] audioSource.second->mOggBuffer;
                    fclose(f);
                    delete[] vi;

                    if (alGetError() != AL_NO_ERROR)
                        throw std::runtime_error("OpenAL Error: [" + std::to_string(alGetError()) + "] in " + audioSource.first + " file!");

                    return;
                }
                case AudioFileFormat::MP3:
                {
                    mp3dec_file_info_t info;
                    mp3dec_load(&s_Mp3d, audioSource.first.c_str(), &info, nullptr, nullptr);
                    audioSource.second->mSize = info.samples * sizeof(mp3d_sample_t);

                    audioSource.second->mMp3Buffer = info.buffer;
                    audioSource.second->mSampleRate = info.hz;
                    audioSource.second->mChannels = info.channels;
                    audioSource.second->mAlFormat = GetOpenALFormat(audioSource.second->mChannels);
                    audioSource.second->mTotalDuration = audioSource.second->mSize / (info.avg_bitrate_kbps * 1024.0f);
                    audioSource.second->mLoaded = true;
                    return;
                }
                default: return;
                }
            }));
        }

        for (uint32_t i{}; i < s_AudioSources.size(); ++i)
        {
            auto& audioSource = s_AudioSources[i];
            futures[i].wait();
            switch (GetFileFormat(audioSource.first))
            {
            case AudioFileFormat::MP3:
            {
                alGenBuffers(1, &audioSource.second->mBufferHandle);
                alBufferData(audioSource.second->mBufferHandle, audioSource.second->mAlFormat, audioSource.second->mMp3Buffer, audioSource.second->mSize, audioSource.second->mSampleRate);
                alGenSources(1, &audioSource.second->mSourceHandle);
                alSourcei(audioSource.second->mSourceHandle, AL_BUFFER, audioSource.second->mBufferHandle);

                if (alGetError() != AL_NO_ERROR) {
                    throw std::runtime_error("OpenAL Error: [" + std::to_string(alGetError()) + "] in " + audioSource.first + " file!");
                }

                delete audioSource.second->mMp3Buffer;
            }
            default: break;
            }
        }

        futures.clear();
        s_AudioSources.clear();

        return true;
    }

    void Shutdown()
    {
        CloseAL();
    }

    void SetGlobalVolume(float volume)
    {
        alListenerf(AL_GAIN, volume);
    }

    Source::Source() = default;

    Source::Source(const std::string& filename)
    {
        LoadFromFile(filename);
    }

    Source::~Source()
    {
        alDeleteSources(1, &mSourceHandle);
        alDeleteBuffers(1, &mBufferHandle);
    }

    bool Source::LoadFromFile(const std::string& filename)
    {
        s_AudioSources.emplace_back(filename, this);
        return true;
    }

    bool Source::IsLoaded() const
    {
        return mLoaded;
    }

    bool Source::IsPlaying() const
    {
        ALenum state;
        alGetSourcei(mSourceHandle, AL_SOURCE_STATE, &state);
        return state == AL_PLAYING;
    }

    bool Source::IsPaused() const
    {
        ALenum state;
        alGetSourcei(mSourceHandle, AL_SOURCE_STATE, &state);
        return state == AL_PAUSED;
    }

    bool Source::IsStopped() const
    {
        ALenum state;
        alGetSourcei(mSourceHandle, AL_SOURCE_STATE, &state);
        return state == AL_STOPPED;
    }

    void Source::Play() const
    {
        // Play the sound until it finishes
        alSourcePlay(mSourceHandle);

        // TODO: current playback time and playback finished callback
    }

    void Source::Pause() const
    {
        alSourcePause(mSourceHandle);
    }

    void Source::Stop() const
    {
        alSourceStop(mSourceHandle);
    }

    void Source::SetPosition(float x, float y, float z)
    {
        mPosition[0] = x;
        mPosition[1] = y;
        mPosition[2] = z;

        alSourcefv(mSourceHandle, AL_POSITION, mPosition);
    }

    void Source::SetGain(float gain)
    {
        mGain = gain;

        alSourcef(mSourceHandle, AL_GAIN, gain);
    }

    void Source::SetPitch(float pitch)
    {
        mPitch = pitch;

        alSourcef(mSourceHandle, AL_PITCH, pitch);
    }

    void Source::SetSpatial(bool spatial)
    {
        mSpatial = spatial;

        alSourcei(mSourceHandle, AL_SOURCE_SPATIALIZE_SOFT, spatial ? AL_TRUE : AL_FALSE);
        alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
    }

    void Source::SetLoop(bool loop)
    {
        mLoop = loop;

        alSourcei(mSourceHandle, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
    }

    [[maybe_unused]] std::pair<uint32_t, uint32_t> Source::GetLengthMinutesAndSeconds() const
    {
        return {static_cast<uint32_t>(mTotalDuration / 60.0f), static_cast<uint32_t>(mTotalDuration) % 60};
    }

    void Source::SetVolume(float volume)
    {
        alSourcef(mSourceHandle, AL_GAIN, volume);
    }
} // namespace Hazel::Audio
