#pragma once

#pragma once

#include <iostream>
#include <string>

namespace Hazel::Audio
{
    bool Init();
    void Shutdown();

    void SetGlobalVolume(float volume);

    class Source
    {
    public:
        Source();
        Source(const Source&&) = delete;
        Source(const Source&) = delete;
        explicit Source(const std::string& filename);
        ~Source();

        bool LoadFromFile(const std::string& filename);

        void Play() const;
        void Pause() const;
        void Stop() const;

        void SetPosition(float x, float y, float z);
        void SetGain(float gain);
        void SetPitch(float pitch);
        void SetSpatial(bool spatial);
        void SetLoop(bool loop);
        void SetVolume(float volume);

        [[nodiscard]] bool IsLoaded() const;
        [[nodiscard]] bool IsPlaying() const;
        [[nodiscard]] bool IsPaused() const;
        [[nodiscard]] bool IsStopped() const;

        [[nodiscard]] std::pair<uint32_t, uint32_t> GetLengthMinutesAndSeconds() const;

    private:
        bool LoadOgg(const std::string& filename);
        bool LoadMp3(const std::string& filename);

        uint32_t mBufferHandle{};
        uint32_t mSourceHandle{};
        bool mLoaded{};
        bool mSpatial{};

        float mTotalDuration{}; // in seconds

        // Attributes
        float mPosition[3]{};
        float mGain{1.0f};
        float mPitch{1.0f};
        bool mLoop{};
    };
} // namespace Hazel::Audio