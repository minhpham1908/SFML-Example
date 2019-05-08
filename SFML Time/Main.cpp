#include <iostream>
#include <SFML/System.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/OpenGL.hpp>
#include <SFML/Audio.hpp>

#include <atomic>
#include <cassert>
#include <mutex>
#include <queue>

using namespace std;

struct Samples {
	Samples(sf::Int16 const* ss, std::size_t count) {
		samples.reserve(count);
		std::copy_n(ss, count, std::back_inserter(samples));
	}

	Samples() {}

	std::vector<sf::Int16> samples;
};



class PlaybackRecorder : public sf::SoundRecorder, private sf::SoundStream {
public: /** API **/

	// Initialise capturing input & setup output
	void start() {
		sf::SoundRecorder::start();
		sf::SoundStream::initialize(sf::SoundRecorder::getChannelCount(), sf::SoundRecorder::getSampleRate());
		sf::SoundStream::play();
	}

	// Stop both recording & playback
	void stop() {
		sf::SoundRecorder::stop();
		sf::SoundStream::stop();
	}

	bool isRunning() { return isRecording; }


	~PlaybackRecorder() {
		stop();
	}


protected: /** OVERRIDING SoundRecorder **/

	bool onProcessSamples(sf::Int16 const* samples, std::size_t sampleCount) override {
		{
			std::lock_guard<std::mutex> lock(mutex);
			data.emplace(samples, sampleCount);
		}
		cv.notify_one();
		return true; // continue capture
	}

	bool onStart() override {
		isRecording = true;
		return true;
	}

	void onStop() override {
		isRecording = false;
		cv.notify_one();
	}


protected: /** OVERRIDING SoundStream **/

	bool onGetData(Chunk& chunk) override {
		// Wait until either:
		//  a) the recording was stopped
		//  b) new data is available
		std::unique_lock<std::mutex> lock(mutex);
		cv.wait(lock, [this] { return !isRecording || !data.empty(); });

		// Lock was acquired, examine which case we're into:
		if (!isRecording) return false; // stop playing.
		else {
			assert(!data.empty());

			playingSamples.samples = std::move(data.front().samples);
			data.pop();
			chunk.sampleCount = playingSamples.samples.size();
			chunk.samples = playingSamples.samples.data();
			return true;
		}
	}

	void onSeek(sf::Time) override { /* Not supported, silently does nothing. */ }

private:
	std::atomic<bool> isRecording{ false };
	std::mutex mutex; // protects `data`
	std::condition_variable cv; // notify consumer thread of new samples
	std::queue<Samples> data; // samples come in from the recorder, and popped by the output stream
	Samples playingSamples; // used by the output stream.
};



int main()
{
	sf::RenderWindow window;;
	window.create(sf::VideoMode(600, 200), "Sound test", sf::Style::Default);
	window.setFramerateLimit(60);


	if (!sf::SoundBufferRecorder::isAvailable())
	{
		return EXIT_FAILURE;
	}
	else {
		cout << "May ban co the thu am" << endl;
	}
	vector<string> availableDevices = sf::SoundRecorder::getAvailableDevices();
	std::string inputDevice = availableDevices[0];
	PlaybackRecorder recorder;
	recorder.setDevice(inputDevice);
	// set the device
	recorder.start();

	while (window.isOpen()) {
		sf::Event event;
		while (window.pollEvent(event)) {
			switch (event.type) {
			case sf::Event::Closed: {
				window.close();
			}
			case sf::Event::Resized: {
				std::cout << "new width: " << event.size.width << std::endl;
				std::cout << "new height: " << event.size.height << std::endl;
			}
			case sf::Event::KeyPressed: {
				if (recorder.isRunning()) {
					recorder.stop();
				}
				else recorder.start();
			}
			default:
				break;
			}
			

		}
		window.clear(recorder.isRunning() ? sf::Color::White : sf::Color::Black);
		window.display();

	}
	system("pause");

	return EXIT_SUCCESS;
}
