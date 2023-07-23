#include "DaisyDuino.h"
#include <DaisyDSP.h>
#include <AudioClass.h>

#include "filters/AntiJitterFilter.h"
#include "filters/ButterworthLPF.h"
#include "TankVerb.h"
#include "FootSwitch.h"

constexpr float maxCutoffFrequency = 20000.f;
constexpr float minCutoffFrequency = 500.f;
constexpr long refreshMs = 20;
constexpr float refreshRate = 1.f / (refreshMs * 0.001f);
constexpr float refreshPeriod = 1.f / refreshRate;
constexpr long readResolution = 16;
constexpr float resolutionScaleFactor = 1 << readResolution;

constexpr int BOOT_SW = D22;
constexpr int FOOT_SW = D20;
constexpr int LED_PIN = D19;

DSPFilters::Butterworth kKnobRoomSizeSmoothing{ 2.f, 50.f };
DSPFilters::Butterworth kKnobSpreadSmoothing{ 2.f, 50.f };
DSPFilters::Butterworth kKnobFrequencySmoothing{ 2.f, 50.f };

AntiJitterFilter kKnobRoomSizeAntiJitter(0.0007f);
AntiJitterFilter kKnobSpreadAntiJitter(0.0007f);
AntiJitterFilter kKnobFrequencyAntiJitter(0.0007f);

DaisyHardware hw;
TankVerb tankVerb;
FootSwitch footSwitch(FOOT_SW, INPUT_PULLDOWN, refreshRate);

float linToExp(float value)
{
	return expf(2 * value - 1.854) - 0.1565;
}

void MyCallback(float** in, float** out, size_t size) {
	float osc_out, env_out;
	//Serial.println(size);
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
	Serial.begin(115200);

	pinMode(LED_PIN, OUTPUT);
	pinMode(BOOT_SW, INPUT_PULLDOWN);

	analogReadResolution(readResolution);
	// Initialize for Daisy pod at 48kHz
	hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
	DAISY.SetAudioBlockSize(AUDIO_BLOCK_SIZE);
	DAISY.begin(MyCallback);
}

void loop()
{
	footSwitch.update();
	digitalWrite(LED_PIN, footSwitch.getToggleState());
	float expression = analogRead(A6) / resolutionScaleFactor;

	tankVerb.setGain(analogRead(A0) / (0.9f * resolutionScaleFactor));

	auto filteredRoomSize = kKnobRoomSizeSmoothing.update(analogRead(A1) / resolutionScaleFactor);
	filteredRoomSize = kKnobRoomSizeAntiJitter.update(filteredRoomSize);

	float frequency = linToExp(analogRead(A3) / resolutionScaleFactor);
	frequency = kKnobFrequencySmoothing.update(frequency);
	frequency = kKnobFrequencyAntiJitter.update(frequency) * maxCutoffFrequency + minCutoffFrequency;
	tankVerb.setCutoff(frequency);

	const float length = linToExp(filteredRoomSize) * MAX_DELAY_LENGTH + AUDIO_BLOCK_SIZE;
	tankVerb.setLength(length);
	Serial.println(length);

	Serial.print(",");

	auto filteredSpread = kKnobSpreadSmoothing.update(analogRead(A2) / resolutionScaleFactor);
	filteredSpread = kKnobSpreadAntiJitter.update(filteredSpread);
	Serial.print(filteredSpread*1000);
	tankVerb.setSpread(filteredSpread);
	//Serial.println();

	//Serial.println(fs);
	delay(refreshMs);
}