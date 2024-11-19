#pragma once

#include<cstdint>

class GameTimer {
public:
	GameTimer();

public:
	float TotalTime() const;		// in seconds
	float DeltaTime() const;		// in seconds

	void Reset();		// Call before message loop.
	void Start();		// Call when unpaused.
	void Stop();		// Call when paused.
	void Tick();		// Call every frame.

private:
	double m_SecondsPerCount;
	double m_DeltaTime;

	int64_t m_BaseTime;
	int64_t m_PausedTime;
	int64_t m_StopTime;
	int64_t m_PrevTime;
	int64_t m_CurrTime;

	bool m_Stopped;
};
