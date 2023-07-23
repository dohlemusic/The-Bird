#pragma once

class FootSwitch
{
public:
	FootSwitch(int pin, int mode, float refreshRate, float division = 4.f)
	{
		mRefreshRate = refreshRate;
		mMult = 1.f/division;
		mPin = pin;
		pinMode(pin, mode);
		mTimerIncrement = refreshRate * mMult;
	}

	void update()
	{
		mTimer += mTimerIncrement;
		mAccumulator += digitalRead(mPin);

		if(mTimer > mRefreshRate)
		{
			const bool newState = mAccumulator * mMult > 0.5f;
			if(newState == true && mLastState == false)
			{
				mToggleState = !mToggleState;
			}
			mLastState = newState;
			mTimer = 0;
			mAccumulator = 0;
		} 
	}

	bool getState()
	{
		return mLastState;
	}

	bool getToggleState()
	{
		return mToggleState;
	}

private:
	int mPin;
	int mAccumulator = 0;
	bool mLastState = false;
	bool mToggleState = false;
	float mTimer = 0;
	float mRefreshRate = 0;
	float mTimerIncrement = 0;
	float mMult = 0;
};