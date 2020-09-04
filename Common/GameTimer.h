#pragma once

class GameTimer {

public:
	
	GameTimer ();

	float TotalTime() const;		// in seconds
	float DeltaTime() const;		// in seconds

	void Reset ();		// Call before message loop.
	void Start ();		// Call when unpaused.
	void Stop ();		// Call when paused.
	void Tick ();		// Call every frame.

private:

	double m_SecondsPerCount;
	double m_DeltaTime;

	__int64 m_BaseTime;
	__int64 m_PausedTime;
	__int64 m_StopTime;
	__int64 m_PrevTime;
	__int64 m_CurrTime;

	bool m_Stopped;

};

