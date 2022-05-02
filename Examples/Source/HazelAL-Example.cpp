#include <HazelAudio/HazelAudio.h>

#include <thread>
#include <chrono>
#include <iostream>

int main()
{
	using namespace std::literals::chrono_literals;
	// Initialize the audio engine
	Hazel::Audio::Init();
	// Load audio source from file
	auto source = Hazel::AudioSource::LoadFromFile("Assets/BackgroundMusic.mp3", false);
	// Make it loop forever
	source.SetLoop(true);
	// Play audio source
	source.Play();
	// Set volume of audio source
	source.SetVolume(0.5f);
	// You can pause audio playing
	source.Pause();
	std::cout << "Is paused: " << source.IsPaused() << '\n';
	std::this_thread::sleep_for(250ms);
	// Or stop
	source.Play();
	source.Stop();
	std::cout << "Is stopped: " << source.IsStopped() << '\n';
	std::this_thread::sleep_for(250ms);
	// Play audio again. (make gamedev great again haha)
	source.Play();
	std::cout << "Is playing: " << source.IsPlaying() << '\n';

	auto frontLeftSource = Hazel::AudioSource::LoadFromFile("Assets/FrontLeft.ogg", true);
	frontLeftSource.SetGain(5.0f);
	frontLeftSource.SetPosition(-5.0f, 0.0f, 5.0f);

	auto frontRightSource = Hazel::AudioSource::LoadFromFile("Assets/FrontRight.ogg", true);
	frontRightSource.SetGain(5.0f);
	frontRightSource.SetPosition(5.0f, 0.0f, 5.0f);

	auto movingSource = Hazel::AudioSource::LoadFromFile("Assets/Moving.ogg", true);
	movingSource.SetGain(5.0f);
	movingSource.SetPosition(5.0f, 0.0f, 5.0f);

	int sourceIndex = 0;
	const int sourceCount = 3;
	Hazel::AudioSource* sources[] = { &frontLeftSource, &frontRightSource, &movingSource };

	float xPosition = 5.0f;
	const float playFrequency = 3.0f; // play sounds every 3 seconds
	float timer = playFrequency;
	std::chrono::steady_clock::time_point lastTime = std::chrono::steady_clock::now();
	while (true)
	{
		std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
		std::chrono::duration<float> delta = currentTime - lastTime;
		lastTime = currentTime;

		if (timer < 0.0f)
		{
			timer = playFrequency;
			if (!sources[sourceIndex % sourceCount]->IsPlaying() && !movingSource.IsPlaying())
				sources[sourceIndex++ % sourceCount]->Play();
		}

		// Moving sound source
		if (sourceIndex == 3)
		{
			xPosition -= delta.count() * 2.0f;
			movingSource.SetPosition(xPosition, 0.0f, 5.0f);
		}
		else
		{
			xPosition = 5.0f;
		}

		timer -= delta.count();

		using namespace std::literals::chrono_literals;
		std::this_thread::sleep_for(5ms);
	}
}
