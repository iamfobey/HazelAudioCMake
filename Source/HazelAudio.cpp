#include "HazelAudio/HazelAudio.h"

#include <filesystem>
#include <string>
#include <thread>
#include <cassert>

#include "al.h"
#include "alc.h"
#include "alext.h"
#include "alhelpers.h"

#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"
#include "minimp3_ex.h"

#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"

#define ASSERTMSG(exp, msg) assert(((void)msg, exp));

namespace Hazel::Audio
{
    static ALCdevice* s_AudioDevice{};
    static mp3dec_t s_Mp3d;

    static uint8_t* s_AudioScratchBuffer;
    static uint32_t s_AudioScratchBufferSize = 10 * 1024 * 1024; // 10mb initially

    // Currently supported file formats
    enum class AudioFileFormat
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

    static ALenum GetOpenAlFormat(uint32_t channels)
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

    bool Init()
    {
        if (InitAL(s_AudioDevice, nullptr, nullptr) != 0)
            return false;

        mp3dec_init(&s_Mp3d);

        s_AudioScratchBuffer = new uint8_t[s_AudioScratchBufferSize];

        // Init listener
        constexpr ALfloat listenerPos[] = {0.0, 0.0, 0.0};
        constexpr ALfloat listenerVel[] = {0.0, 0.0, 0.0};
        constexpr ALfloat listenerOri[] = {0.0, 0.0, -1.0, 0.0, 1.0, 0.0};
        alListenerfv(AL_POSITION, listenerPos);
        alListenerfv(AL_VELOCITY, listenerVel);
        alListenerfv(AL_ORIENTATION, listenerOri);

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

    bool Source::LoadOgg(const std::string& filename)
    {
        FILE* f = fopen(filename.c_str(), "rb");

        OggVorbis_File vf;
        if (ov_open_callbacks(f, &vf, nullptr, 0, OV_CALLBACKS_NOCLOSE) < 0)
            return false;

        vorbis_info* vi = ov_info(&vf, -1);
        const auto sampleRate = vi->rate;
        const auto channels = vi->channels;
        const auto alFormat = GetOpenAlFormat(channels);
        const auto samples = ov_pcm_total(&vf, -1);
        const auto bufferSize = 2 * channels * samples; // 2 bytes per sample (I'm guessing...)

        if (s_AudioScratchBufferSize < bufferSize)
        {
            s_AudioScratchBufferSize = bufferSize;
            delete[] s_AudioScratchBuffer;
            s_AudioScratchBuffer = new uint8_t[s_AudioScratchBufferSize];
        }

        uint8_t* oggBuffer = s_AudioScratchBuffer;
        uint8_t* bufferPtr = oggBuffer;
        while (true)
        {
            int currentSection{};
            const auto length = ov_read(&vf, reinterpret_cast<char*>(bufferPtr), 4096, 0, 2, 1, &currentSection);
            bufferPtr += length;
            if (length == 0)
                break;
            else if (length < 0)
                ASSERTMSG(length == OV_EBADLINK, "Corrupt bitstream section!")
        }

        const auto size = bufferPtr - oggBuffer;
        ASSERTMSG(bufferSize == size, "Buffer size equals size of ogg buffer!")

        // Release file
        ov_clear(&vf);
        fclose(f);

        alGenBuffers(1, &mBufferHandle);
        alBufferData(mBufferHandle, alFormat, oggBuffer, static_cast<int>(size), static_cast<int>(sampleRate));
        alGenSources(1, &mSourceHandle);
        alSourcei(mSourceHandle, AL_BUFFER, static_cast<int>(mBufferHandle));

        if (alGetError() != AL_NO_ERROR)
            return false;

        mTotalDuration = static_cast<float>(samples) / static_cast<float>(sampleRate); // in seconds
        mLoaded = true;

        return true;
    }

    bool Source::LoadMp3(const std::string& filename)
    {
        mp3dec_file_info_t info;
        mp3dec_load(&s_Mp3d, filename.c_str(), &info, nullptr, nullptr);
        const auto size = info.samples * sizeof(mp3d_sample_t);

        const auto sampleRate = info.hz;
        const auto channels = info.channels;
        const auto alFormat = GetOpenAlFormat(channels);

        alGenBuffers(1, &mBufferHandle);
        alBufferData(mBufferHandle, alFormat, info.buffer, static_cast<int>(size), sampleRate);
        alGenSources(1, &mSourceHandle);
        alSourcei(mSourceHandle, AL_BUFFER, static_cast<int>(mBufferHandle));

        if (alGetError() != AL_NO_ERROR)
            return false;

        mTotalDuration = static_cast<float>(size) / (static_cast<float>(info.avg_bitrate_kbps) * 1024.0f);
        mLoaded = true;

        return true;
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
        switch (GetFileFormat(filename))
        {
        case AudioFileFormat::Ogg: return LoadOgg(filename);
        case AudioFileFormat::MP3: return LoadMp3(filename);
        case AudioFileFormat::None: return true;
        }
        return false;
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
        alSourcePlay(mSourceHandle);
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

    std::pair<uint32_t, uint32_t> Source::GetLengthMinutesAndSeconds() const
    {
        return {static_cast<uint32_t>(mTotalDuration / 60.0f), static_cast<uint32_t>(mTotalDuration) % 60};
    }

#pragma clang diagnostic push
#pragma ide diagnostic ignored "readability-make-member-function-const"
    void Source::SetVolume(float volume)
    {
        alSourcef(mSourceHandle, AL_GAIN, volume);
    }
#pragma clang diagnostic pop
} // namespace Hazel::Audio