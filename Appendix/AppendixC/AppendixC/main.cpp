#include <windows.h>
#include <iostream>
#include <DirectXMath.h>

using namespace std;
using namespace DirectX;

ostream& operator<<(ostream& os, FXMVECTOR v) {
	XMFLOAT4 dest;
	XMStoreFloat4(&dest, v);

	os << "(" << dest.x << ", " << dest.y << ", " << dest.z << ", " << dest.w << ")";
	return os;
}

int main() {
	XMVECTOR p0 = XMVectorSet(-1.0f, 1.0f, -1.0f, 1.0f);
	XMVECTOR u = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

	float s = 1.0f / sqrtf(3);
	XMVECTOR plane = XMVectorSet(s, s, s, -5.0f);

	XMVECTOR isect = XMPlaneIntersectLine(plane, p0, p0 + 100 * u);

	cout << isect << endl;

	return 0;
}
