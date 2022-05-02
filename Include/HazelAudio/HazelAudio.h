#pragma once

#include <iostream>
#include <string>

namespace Hazel::Audio
{
	void Init();
	void Shutdown();

	class Source
	{
	public:
		Source();
		~Source();

		void LoadFromFile(const std::string& filename);

		void Play();
		void Pause();
		void Stop();

		void SetPosition(float x, float y, float z);
		void SetGain(float gain);
		void SetPitch(float pitch);
		void SetSpatial(bool spatial);
		void SetLoop(bool loop);
		void SetVolume(float volume);

		bool IsLoaded() const;
		bool IsPlaying() const;
		bool IsPaused() const;
		bool IsStopped() const;

		[[nodiscard]] std::pair<uint32_t, uint32_t> GetLengthMinutesAndSeconds() const;
	private:
		void LoadOgg(const std::string& filename);
		void LoadMp3(const std::string& filename);

		uint32_t m_BufferHandle{};
		uint32_t m_SourceHandle{};
		bool m_Loaded{};
		bool m_Spatial{};

		float m_TotalDuration{}; // in seconds

		// Attributes
		float m_Position[3]{};
		float m_Gain{ 1.0f };
		float m_Pitch{ 1.0f };
		bool m_Loop{};
	};
}