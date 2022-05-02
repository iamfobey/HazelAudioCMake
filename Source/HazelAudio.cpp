#include "HazelAudio/HazelAudio.h"

#include <string>
#include <thread>
#include <filesystem>

#include "al.h"
#include "alc.h"
#include "alext.h"
#include "HazelAudio/alhelpers.h"

#define MINIMP3_IMPLEMENTATION
#include <cassert>

#include "minimp3.h"
#include "minimp3_ex.h"

#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"

namespace Hazel::Audio
{
	static ALCdevice* s_AudioDevice{};
	static mp3dec_t s_Mp3d;

	static uint8_t* s_AudioScratchBuffer;
	static uint32_t s_AudioScratchBufferSize = 10 * 1024 * 1024; // 10mb initially
	
#define HA_LOG(x) std::cout << "[Hazel Audio]  " << x << std::endl

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

		if (extension == ".ogg")  return AudioFileFormat::Ogg;
		if (extension == ".mp3")  return AudioFileFormat::MP3;
		
		return AudioFileFormat::None;
	}

	static ALenum GetOpenALFormat(uint32_t channels)
	{
		// Note: sample size is always 2 bytes (16-bits) with
		// both the .mp3 and .ogg decoders that we're using
		switch (channels)
		{
			case 1:  return AL_FORMAT_MONO16;
			case 2:  return AL_FORMAT_STEREO16;
		}
		// assert
		return 0;
	}

	void Audio::Init()
	{
		if (InitAL(s_AudioDevice, nullptr, 0) != 0)
			std::cout << "Audio device error!\n";

		mp3dec_init(&s_Mp3d);

		s_AudioScratchBuffer = new uint8_t[s_AudioScratchBufferSize];

		// Init listener
		constexpr ALfloat listenerPos[] = { 0.0,0.0,0.0 };
		constexpr ALfloat listenerVel[] = { 0.0,0.0,0.0 };
		constexpr ALfloat listenerOri[] = { 0.0, 0.0, -1.0, 0.0, 1.0, 0.0 };
		alListenerfv(AL_POSITION, listenerPos);
		alListenerfv(AL_VELOCITY, listenerVel);
		alListenerfv(AL_ORIENTATION, listenerOri);
	}

	void Shutdown() { CloseAL(); }

	void Source::LoadOgg(const std::string& filename)
	{
		FILE* f = fopen(filename.c_str(), "rb");

		OggVorbis_File vf;
		if (ov_open_callbacks(f, &vf, NULL, 0, OV_CALLBACKS_NOCLOSE) < 0)
			std::cout << "Could not open ogg stream\n";

		vorbis_info* vi = ov_info(&vf, -1);
		auto sampleRate = vi->rate;
		auto channels = vi->channels;
		auto alFormat = GetOpenALFormat(channels);
		uint64_t samples = ov_pcm_total(&vf, -1);
		uint32_t bufferSize = 2 * channels * samples; // 2 bytes per sample (I'm guessing...)

		m_TotalDuration = static_cast<float>(samples) / static_cast<float>(sampleRate); // in seconds
		m_Loaded = true;

		// TODO: Replace with Hazel::Buffer
		if (s_AudioScratchBufferSize < bufferSize)
		{
			s_AudioScratchBufferSize = bufferSize;
			delete[] s_AudioScratchBuffer;
			s_AudioScratchBuffer = new uint8_t[s_AudioScratchBufferSize];
		}

		uint8_t* oggBuffer = s_AudioScratchBuffer;
		uint8_t* bufferPtr = oggBuffer;
		int eof = 0;
		while (!eof)
		{
			int current_section;
			long length = ov_read(&vf, reinterpret_cast<char*>(bufferPtr), 4096, 0, 2, 1, &current_section);
			bufferPtr += length;
			if (length == 0) eof = 1;
			else if (length < 0)
			{
				if (length == OV_EBADLINK)
				{
					fprintf(stderr, "Corrupt bitstream section! Exiting.\n");
					exit(1);
				}
			}
		}

		uint32_t size = bufferPtr - oggBuffer;
		assert(bufferSize == size);

		// Release file
		ov_clear(&vf);
		fclose(f);

		alGenBuffers(1, &m_BufferHandle);
		alBufferData(m_BufferHandle, alFormat, oggBuffer, size, sampleRate);
		alGenSources(1, &m_SourceHandle);
		alSourcei(m_SourceHandle, AL_BUFFER, m_BufferHandle);

		if (alGetError() != AL_NO_ERROR)
			HA_LOG("Failed to setup sound source");
	}

	void Source::LoadMp3(const std::string& filename)
	{
		mp3dec_file_info_t info;
		mp3dec_load(&s_Mp3d, filename.c_str(), &info, nullptr, nullptr);
		const uint32_t size = info.samples * sizeof(mp3d_sample_t);

		const auto sampleRate = info.hz;
		const auto channels = info.channels;
		const auto alFormat = GetOpenALFormat(channels);
		m_TotalDuration = size / (info.avg_bitrate_kbps * 1024.0f);
		m_Loaded = true;

		alGenBuffers(1, &m_BufferHandle);
		alBufferData(m_BufferHandle, alFormat, info.buffer, size, sampleRate);
		alGenSources(1, &m_SourceHandle);
		alSourcei(m_SourceHandle, AL_BUFFER, m_BufferHandle);
		
		if (alGetError() != AL_NO_ERROR)
			std::cout << "Failed to setup sound source" << std::endl;
	}


	Source::Source() = default;

	Source::~Source()
	{
		alDeleteSources(1, &m_SourceHandle);
	}

	void Source::LoadFromFile(const std::string& filename)
	{
		switch (GetFileFormat(filename))
		{
			case AudioFileFormat::Ogg:  return LoadOgg(filename);
			case AudioFileFormat::MP3:  return LoadMp3(filename);
			case AudioFileFormat::None: break;
		}

	}

	bool Source::IsLoaded() const { return m_Loaded; }

	bool Source::IsPlaying() const
	{
		ALenum state;
		alGetSourcei(m_SourceHandle, AL_SOURCE_STATE, &state);
		return state == AL_PLAYING;
	}

	bool Source::IsPaused() const
	{
		ALenum state;
		alGetSourcei(m_SourceHandle, AL_SOURCE_STATE, &state);
		return state == AL_PAUSED;
	}

	bool Source::IsStopped() const
	{
		ALenum state;
		alGetSourcei(m_SourceHandle, AL_SOURCE_STATE, &state);
		return state == AL_STOPPED;
	}

	void Source::Play()
	{
		// Play the sound until it finishes
		alSourcePlay(m_SourceHandle);

		// TODO: current playback time and playback finished callback
		// eg.
		// ALfloat offset;
		// alGetSourcei(audioSource.m_SourceHandle, AL_SOURCE_STATE, &s_PlayState);
		// ALenum s_PlayState;
		// alGetSourcef(audioSource.m_SourceHandle, AL_SEC_OFFSET, &offset);
	}

	void Source::Pause()
	{
		alSourcePause(m_SourceHandle);
	}

	void Source::Stop()
	{
		alSourceStop(m_SourceHandle);
	}

	void Source::SetPosition(float x, float y, float z)
	{
		m_Position[0] = x;
		m_Position[1] = y;
		m_Position[2] = z;

		alSourcefv(m_SourceHandle, AL_POSITION, m_Position);
	}

	void Source::SetGain(float gain)
	{
		m_Gain = gain;

		alSourcef(m_SourceHandle, AL_GAIN, gain);
	}

	void Source::SetPitch(float pitch)
	{
		m_Pitch = pitch;

		alSourcef(m_SourceHandle, AL_PITCH, pitch);
	}

	void Source::SetSpatial(bool spatial)
	{
		m_Spatial = spatial;

		alSourcei(m_SourceHandle, AL_SOURCE_SPATIALIZE_SOFT, spatial ? AL_TRUE : AL_FALSE);
		alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
	}

	void Source::SetLoop(bool loop)
	{
		m_Loop = loop;

		alSourcei(m_SourceHandle, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
	}

	std::pair<uint32_t, uint32_t> Source::GetLengthMinutesAndSeconds() const
	{
		return { static_cast<uint32_t>(m_TotalDuration / 60.0f), static_cast<uint32_t>(m_TotalDuration) % 60 };
	}

	void Source::SetVolume(float volume)
	{
		alSourcef(m_SourceHandle, AL_GAIN, volume);
	}
}
