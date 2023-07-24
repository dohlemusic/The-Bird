#include "DaisyDuino.h"
#include <DaisyDSP.h>
#include <AudioClass.h>

#include "filters/AntiJitterFilter.h"
#include "filters/ButterworthLPF.h"
#include "TankVerb.h"
#include "FootSwitch.h"

enum KnobType {FEEDBACK, LENGTH, SPREAD, CUTOFF, COUNT};
int lastKnobState[KnobType::COUNT] = {0};
constexpr int knobDefaultPins[KnobType::COUNT] = {A0, A1, A2, A3};
int knobPinAssignment[KnobType::COUNT] = {A0, A1, A2, A3};

enum class PedalState {RUNNING, KNOB_ASSIGNMENT};
PedalState currentState = PedalState::RUNNING;

constexpr float maxCutoffFrequency = 20000.f;
constexpr float minCutoffFrequency = 500.f;
constexpr long refreshMs = 10;
constexpr float refreshRate = 1.f / (refreshMs * 0.001f);
constexpr float refreshPeriod = 1.f / refreshRate;
constexpr long readResolution = 16;
constexpr float resolutionScaleFactor = 1 << readResolution;

constexpr int ASSIGN_SW = D22;
constexpr int EXPRESSION_PIN = A6;
constexpr int FOOT_SW = D20;
constexpr int LED_PIN = D19;

DSPFilters::Butterworth kKnobRoomSizeSmoothing{ 2.f, 50.f };
DSPFilters::Butterworth kKnobSpreadSmoothing{ 2.f, 50.f };
DSPFilters::Butterworth kKnobFrequencySmoothing{ 2.f, 50.f };

AntiJitterFilter kKnobRoomSizeAntiJitter(0.001f);
AntiJitterFilter kKnobSpreadAntiJitter(0.001f);
AntiJitterFilter kKnobFrequencyAntiJitter(0.001f);

DaisyHardware hw;
TankVerb tankVerb;
FootSwitch footSwitch(FOOT_SW, INPUT_PULLDOWN, refreshRate);

float linToExp(float value)
{
	return expf(2 * value - 1.854) - 0.1565;
}

void AudioCallback(float** in, float** out, size_t size) {
	float osc_out, env_out;
	if(footSwitch.getToggleState())
	{
		tankVerb.update(in[1], out[1], size);
		out[0] = out[1];
	} else
	{
		out[0] = in[0];
		out[1] = in[1];
	}
}

void setup() {
	Serial.begin(9600);
	pinMode(LED_PIN, OUTPUT);
	pinMode(ASSIGN_SW, INPUT_PULLDOWN);

	analogReadResolution(readResolution);

	// Initialize for Daisy pod at 48kHz
	hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
	DAISY.SetAudioBlockSize(AUDIO_BLOCK_SIZE);
	DAISY.begin(AudioCallback);
}

void run() {
	footSwitch.update();
	digitalWrite(LED_PIN, footSwitch.getToggleState());

	for(int i=0; i<KnobType::COUNT; ++i) {
		lastKnobState[i] = analogRead(knobDefaultPins[i]);
	}

	tankVerb.setGain(analogRead(knobPinAssignment[KnobType::FEEDBACK]) / (0.9f * resolutionScaleFactor));

	auto filteredRoomSize = kKnobRoomSizeSmoothing.update(analogRead(knobPinAssignment[KnobType::LENGTH]) / resolutionScaleFactor);
	filteredRoomSize = kKnobRoomSizeAntiJitter.update(filteredRoomSize);

	float frequency = linToExp(analogRead(knobPinAssignment[KnobType::CUTOFF]) / resolutionScaleFactor);
	frequency = kKnobFrequencySmoothing.update(frequency);
	frequency = kKnobFrequencyAntiJitter.update(frequency) * maxCutoffFrequency + minCutoffFrequency;
	tankVerb.setCutoff(frequency);

	const float length = linToExp(filteredRoomSize) * MAX_DELAY_LENGTH + AUDIO_BLOCK_SIZE;
	tankVerb.setLength(length);

	auto filteredSpread = kKnobSpreadSmoothing.update(analogRead(knobPinAssignment[KnobType::SPREAD]) / resolutionScaleFactor);
	filteredSpread = kKnobSpreadAntiJitter.update(filteredSpread);
	
	tankVerb.setSpread(filteredSpread);
}

void assignPedal() {
	digitalToggle(LED_PIN);
	for(int i=0; i<KnobType::COUNT; ++i) {
		knobPinAssignment[i] = knobDefaultPins[i];
	}

	for(int i=0; i<KnobType::COUNT; ++i) {
		auto value = abs((long long)analogRead(knobPinAssignment[i]) - (long long)lastKnobState[i]);
		Serial.println(value);
		if(value > 0.1f * resolutionScaleFactor) {
			knobPinAssignment[i] = EXPRESSION_PIN;
			currentState = PedalState::RUNNING;
			return;
		}
	}
}

void loop()
{
	if(digitalRead(ASSIGN_SW) == HIGH) {
		if(currentState == PedalState::RUNNING) {
			currentState = PedalState::KNOB_ASSIGNMENT;
		} else {
			currentState = PedalState::RUNNING;
		}
	}

	if(currentState == PedalState::RUNNING) {
		run();
		delay(refreshMs);
	} else {
		assignPedal();
		delay(refreshMs * 10);
	}
}