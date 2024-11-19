#include "GameTimer.h"

#include <windows.h>

GameTimer::GameTimer()
	: _secondsPerCount(0.0), _deltaTime(-1.0), _baseTime(0),
	_pausedTime(0), _stopTime(0), _prevTime(0), _currTime(0), _stopped(false) {
	int64_t countsPerSec;
	QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);
	_secondsPerCount = 1.0 / (double)countsPerSec;
}

float GameTimer::TotalTime() const {
	if (_stopped) {
		return (float)(((_stopTime - _pausedTime) - _baseTime) * _secondsPerCount);
	} else {
		return (float)(((_currTime - _pausedTime) - _baseTime) * _secondsPerCount);
	}
}

float GameTimer::DeltaTime() const {
	return (float)_deltaTime;
}

void GameTimer::Reset() {
	int64_t currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

	_baseTime = currTime;
	_prevTime = currTime;
	_stopTime = 0;
	_stopped = false;
}

void GameTimer::Start() {
	int64_t startTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&startTime);

	if (_stopped) {
		_pausedTime += (startTime - _stopTime);

		_prevTime = startTime;
		_stopTime = 0;
		_stopped = false;
	}
}

void GameTimer::Stop() {
	if (!_stopped) {
		int64_t currTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

		_stopTime = currTime;
		_stopped = true;
	}
}

void GameTimer::Tick() {
	if (_stopped) {
		_deltaTime = 0.0;
		return;
	}

	int64_t currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
	_currTime = currTime;

	_deltaTime = (_currTime - _prevTime) * _secondsPerCount;

	_prevTime = _currTime;

	if (_deltaTime < 0.0) {
		_deltaTime = 0.0;
	}
}
