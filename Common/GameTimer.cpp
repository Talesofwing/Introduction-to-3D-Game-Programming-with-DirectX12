#include "GameTimer.h"

#include <windows.h>

#include <string>

GameTimer::GameTimer () : m_SecondsPerCount (0.0), m_DeltaTime (-1.0), m_BaseTime (0),
						 m_PausedTime (0), m_StopTime (0), m_PrevTime (0), m_CurrTime (0), m_Stopped (false) {
	// ﾃｿﾃ・ﾌﾐﾐｶ猖ﾙｴﾎ
	_int64 countsPerSec;
	QueryPerformanceFrequency ((LARGE_INTEGER*) &countsPerSec);
	// ﾃｿ偉ﾐﾐﾒｻｴﾎﾒｪｶ猖ﾙﾃ・
	m_SecondsPerCount = 1.0 / (double)countsPerSec;
}

float GameTimer::TotalTime () const {
	if (m_Stopped) {
		return (float)(((m_StopTime - m_PausedTime) - m_BaseTime) * m_SecondsPerCount);
	} else {
		return (float)(((m_CurrTime - m_PausedTime) - m_BaseTime) * m_SecondsPerCount);
	}
}

float GameTimer::DeltaTime () const {
	return (float)m_DeltaTime;
}

void GameTimer::Reset () {
	__int64 currTime;
	QueryPerformanceCounter ((LARGE_INTEGER*)&currTime);

	m_BaseTime = currTime;
	m_PrevTime = currTime;
	m_StopTime = 0;
	m_Stopped = false;
}

void GameTimer::Start () {
	__int64 startTime;
	QueryPerformanceCounter ((LARGE_INTEGER*)&startTime);

	if (m_Stopped) {
		m_PausedTime += (startTime - m_StopTime);

		m_PrevTime = startTime;
		m_StopTime = 0;
		m_Stopped = false;
	}
}

void GameTimer::Stop () {
	if (!m_Stopped) {
		__int64 currTime;
		QueryPerformanceCounter ((LARGE_INTEGER*)&currTime);

		m_StopTime = currTime;
		m_Stopped = true;
	}
}

void GameTimer::Tick () {
	if (m_Stopped) {
		m_DeltaTime = 0.0;
		return;
	}
	
	// ｫ@ｵﾃｱｾ汐饑ﾊｼ・ﾊｾｵﾄ瓶ｿﾌ
	__int64 currTime;
	QueryPerformanceCounter ((LARGE_INTEGER*)&currTime);
	m_CurrTime = currTime;

	// ｱｾ汐ﾅcﾇｰﾒｻ汐ｵﾄ瓶馮ｲ・
	m_DeltaTime = (m_CurrTime - m_PrevTime) * m_SecondsPerCount;

	// 慳ゆﾓ桐羈ｾ汐ﾅcﾏﾂﾒｻ汐ｵﾄ瓶馮ｲ・
	m_PrevTime = m_CurrTime;

	// ﾊｹ瓶馮ｲ釚鮃ﾇﾘ敦ｵ｡｣DXSDKﾖﾐｵﾄCDXUTTimerﾊｾﾀﾗ｢瘡ﾑYﾌ盞ｽ: ﾈ郢銧実枻珠ｶｹ敍ﾜﾄ｣ﾊｽ,ｻﾟﾔﾚ
	// ﾓ桐繝ﾉ汐馮瓶馮ｲ鋙ﾄﾟ^ｳﾌﾖﾐﾇﾐ轍ｵｽﾁ橫ｻ・ﾌ実枻r (ｼｴQueryPerformanceCounterｺｯ鳩ｵﾄλｴﾎﾕ{
	// ﾓﾃ゜ｷﾇﾔﾚﾍｬﾒｻ・ﾌ実枻ﾏ), дdeltaTimeﾓﾐｿﾉﾄﾜ・ｳﾉ樣ﾘ敦ｵ
	if (m_DeltaTime < 0.0) {
		m_DeltaTime = 0.0;
	}
}