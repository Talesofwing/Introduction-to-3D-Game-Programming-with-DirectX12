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
	double _secondsPerCount;
	double _deltaTime;

	int64_t _baseTime;
	int64_t _pausedTime;
	int64_t _stopTime;
	int64_t _prevTime;
	int64_t _currTime;

	bool _stopped;
};
