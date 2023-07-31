#include "DaisyDuino.h"
#include <DaisyDSP.h>
#include <AudioClass.h>

#include "filters/AntiJitterFilter.h"
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

const float maxJitterAmount = 0.0007f;
AntiJitterFilter kKnobRoomSizeAntiJitter(maxJitterAmount);
AntiJitterFilter kKnobSpreadAntiJitter(maxJitterAmount);
AntiJitterFilter kKnobFrequencyAntiJitter(maxJitterAmount);

DaisyHardware hw;
TankVerb tankVerb;
FootSwitch footSwitch(FOOT_SW, INPUT_PULLDOWN, refreshRate);

template <class T>
T remap(T in, T inMin, T inMax, T outMin, T outMax)
{
  return (in - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}

float linToExp(float value)
{
	return expf(2 * value - 1.854) - 0.1565;
}

void AudioCallback(float** in, float** out, size_t size) {
	float osc_out, env_out;
	if(footSwitch.getToggleState())
	{
		DAISY.SetAudioBlockSize(AUDIO_BLOCK_SIZE);
		tankVerb.update(in[1], out[1], size);
		out[0] = out[1];
	} else
	{
		DAISY.SetAudioBlockSize(64);
		out[0] = in[0];
		out[1] = in[1];
	}
}

void setup() {
	Serial.begin(9600);
	pinMode(LED_PIN, OUTPUT);
	pinMode(ASSIGN_SW, INPUT_PULLDOWN);

	analogReadResolution(readResolution);

	// Initialize for Daisy seed at 48kHz
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

	float cutoff = linToExp(analogRead(knobPinAssignment[KnobType::CUTOFF]) / resolutionScaleFactor);
	cutoff = linToExp(cutoff); //harder slope
	tankVerb.setCutoff(cutoff * maxCutoffFrequency + minCutoffFrequency);

	float length = analogRead(knobPinAssignment[KnobType::LENGTH]) / resolutionScaleFactor;
	length = linToExp(length);
	length = remap(length, 0.f, 1.f, (float)MIN_DELAY_LENGTH, (float)MAX_DELAY_LENGTH);
	tankVerb.setLength(length);

	auto spread = analogRead(knobPinAssignment[KnobType::SPREAD]) / resolutionScaleFactor;
	// add some extra value, because potentiometers rarely read their max possible value and we'd be missing out on a cool sound effect otherwise
	tankVerb.setSpread(spread + 0.02);
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