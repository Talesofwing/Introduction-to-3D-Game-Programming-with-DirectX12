#pragma once

#include <Windows.h>
#include <DirectXMath.h>
#include <cstdint>

using namespace DirectX;

class MathHelper {
public:
	// Returns random float in [0, 1)
	static float RandF() {
		return (float)(rand()) / (float)RAND_MAX;
	}

	// Return random float in [a, b)
	static float RandF(float a, float b) {
		return a + RandF() * (b - a);
	}

	// Return random int in [a, b]
	static int Rand(int a, int b) {
		return a + rand() % ((b - a) + 1);
	}

	template<typename T>
	static T Max(const T& a, const T& b) {
		return a > b ? a : b;
	}

	template<typename T>
	static T Clamp(const T& x, const T& low, const T& high) {
		return x < low ? low : (x > high ? high : x);
	}

	// Returns the polar angle of the point (x, y) in [0, 2 * pi];
	float AngleFromXY(float x, float y);

	static DirectX::XMVECTOR SphericalToCartesian(float radius, float theta, float phi) {
		return DirectX::XMVectorSet(
			radius * sinf(phi) * cosf(theta),
			radius * cosf(phi),
			radius * sinf(phi) * sinf(theta),
			1.0f
		);
	}

	static XMMATRIX InverseTranspose(CXMMATRIX M) {
		XMMATRIX A = M;
		A.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

		XMVECTOR det = XMMatrixDeterminant(A);
		return XMMatrixTranspose(XMMatrixInverse(&det, A));
	}

	static XMFLOAT4X4 Identity4x4() {
		static XMFLOAT4X4 I(
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		);

		return I;
	}

	static const float Infinity;
	static const float PI;
};
