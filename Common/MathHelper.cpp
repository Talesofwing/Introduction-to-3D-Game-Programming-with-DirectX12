#include "MathHelper.h"

#include <float.h>
#include <cmath>

const float MathHelper::Infinity = FLT_MAX;
const float MathHelper::PI = 3.1415926535f;

float MathHelper::AngleFromXY(float x, float y) {
	float theta = 0.0f;

	// Quadrant I or IV
	if (x >= 0.0f) {
		// If x = 0, then atanf(y/x) = +pi/2 if y > 0
		//                atanf(y/x) = -pi/2 if y < 0
		theta = atanf(y / x); // in [-pi/2, +pi/2]

		if (theta < 0.0f)
			theta += 2.0f * PI; // in [0, 2*pi).
	} else
		// Quadrant II or III
		theta = atanf(y / x) + PI; // in [0, 2*pi).

	return theta;
}
