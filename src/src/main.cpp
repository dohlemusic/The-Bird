#include "daisy_seed.h"

#include "TankVerb.h"
#include "FootSwitch.h"

enum KnobType
{
	FEEDBACK,
	LENGTH,
	SPREAD,
	CUTOFF,
	COUNT
};
int lastKnobState[KnobType::COUNT] = {0};

daisy::GPIO assignSwitch;
daisy::GPIO expressionPedal;
daisy::GPIO foootSwitch;
daisy::GPIO led;

daisy::GPIO feedbackKnob;
daisy::GPIO lengthKnob;
daisy::GPIO spreadKnob;
daisy::GPIO cutoffKnob;

daisy::Pin knobDefaultPins[KnobType::COUNT] = {daisy::seed::A0, daisy::seed::A1, daisy::seed::A2, daisy::seed::A3};
daisy::Pin knobPinAssignment[KnobType::COUNT] = {daisy::seed::A0, daisy::seed::A1, daisy::seed::A2, daisy::seed::A3};

enum class PedalState
{
	RUNNING,
	KNOB_ASSIGNMENT
};
PedalState currentState = PedalState::RUNNING;

constexpr float maxCutoffFrequency = 20000.f;
constexpr float minCutoffFrequency = 500.f;
constexpr long refreshMs = 10;
constexpr float refreshRate = 1.f / (refreshMs * 0.001f);
constexpr float refreshPeriod = 1.f / refreshRate;
constexpr long readResolution = 16;
constexpr float resolutionScaleFactor = 1 << readResolution;

daisy::DaisySeed hw;
TankVerb tankVerb;
FootSwitch footSwitch(foootSwitch, refreshRate);

float linToExp(float value)
{
	return expf(2 * value - 1.854) - 0.1565;
}

void AudioCallback(daisy::AudioHandle::InputBuffer in,
				   daisy::AudioHandle::OutputBuffer out, size_t size)
{
	bool bypass = footSwitch.getToggleState();
	if (!bypass)
	{
		hw.SetAudioBlockSize(AUDIO_BLOCK_SIZE);
		tankVerb.update(in[1], out[1], size);
		out[0] = out[1];
	}
	else
	{
		// reduce the buffer size for minimum delay in bypass mode
		hw.SetAudioBlockSize(64);
		for (size_t i = 0; i < size; i++)
		{
			out[0][i] = in[0][i];
			out[1][i] = in[1][i];
		}
	}
}

void setup()
{
	using namespace daisy;
	assignSwitch.Init(seed::D22, GPIO::Mode::INPUT, GPIO::Pull::PULLUP);
	expressionPedal.Init(seed::A6, GPIO::Mode::ANALOG);
	foootSwitch.Init(seed::D20, GPIO::Mode::INPUT, GPIO::Pull::PULLUP);
	led.Init(seed::D19, GPIO::Mode::OUTPUT, GPIO::Pull::NOPULL);

	// Initialize for Daisy pod at 48kHz
	hw.Init();
	hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
	hw.SetAudioBlockSize(AUDIO_BLOCK_SIZE);
	hw.StartAudio(AudioCallback);
}

void run()
{
	footSwitch.update();
	led.Write(footSwitch.getToggleState());

	//for (int i = 0; i < KnobType::COUNT; ++i)
	//{
	//	lastKnobState[i] = analogRead(knobDefaultPins[i]);
	//}

	//tankVerb.setGain(analogRead(knobPinAssignment[KnobType::FEEDBACK]) / (0.9f * resolutionScaleFactor));
//
	//float frequency = linToExp(analogRead(knobPinAssignment[KnobType::CUTOFF]) / resolutionScaleFactor);
	//frequency = linToExp(frequency); // harder slope
	//tankVerb.setCutoff(frequency * maxCutoffFrequency + minCutoffFrequency);
//
	//auto filteredRoomSize = analogRead(knobPinAssignment[KnobType::LENGTH]) / resolutionScaleFactor;
	//const float length = linToExp(filteredRoomSize) * MAX_DELAY_LENGTH;
	//tankVerb.setLength(length);
//
	//auto filteredSpread = analogRead(knobPinAssignment[KnobType::SPREAD]) / resolutionScaleFactor;
	//tankVerb.setSpread(filteredSpread + 0.01f);
}

//void assignPedal()
//{
//	digitalToggle(LED_PIN);
//	for (int i = 0; i < KnobType::COUNT; ++i)
//	{
//		knobPinAssignment[i] = knobDefaultPins[i];
//	}
//
//	for (int i = 0; i < KnobType::COUNT; ++i)
//	{
//		auto value = abs((long long)analogRead(knobPinAssignment[i]) - (long long)lastKnobState[i]);
//		Serial.println(value);
//		if (value > 0.1f * resolutionScaleFactor)
//		{
//			knobPinAssignment[i] = EXPRESSION_PIN;
//			currentState = PedalState::RUNNING;
//			return;
//		}
//	}
//}

int main()
{
	setup();

	/*
	if (digitalRead(ASSIGN_SW) == HIGH)
	{
		if (currentState == PedalState::RUNNING)
		{
			currentState = PedalState::KNOB_ASSIGNMENT;
		}
		else
		{
			currentState = PedalState::RUNNING;
		}
	}

	if (currentState == PedalState::RUNNING)
	{
		run();
		delay(refreshMs);
	}
	else
	{
		assignPedal();
		delay(refreshMs * 10);
	}
	*/

	while(1)
	{
		run();
	}
}