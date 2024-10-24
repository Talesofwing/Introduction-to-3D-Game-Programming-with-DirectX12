#include <DirectXMath.h>
#include <iostream>

using namespace std;
using namespace DirectX;

ostream& XM_CALLCONV operator << (ostream& os, FXMVECTOR v) {
	XMFLOAT4 dest;
	XMStoreFloat4(&dest, v);

	os << "(" << dest.x << ", " << dest.y << ", " << dest.z << ", " << dest.w << ")";
	return os;
}

ostream& XM_CALLCONV operator << (ostream& os, FXMMATRIX m) {
	for (int i = 0; i < 4; ++i) {
		os << XMVectorGetX(m.r[i]) << "\t";
		os << XMVectorGetY(m.r[i]) << "\t";
		os << XMVectorGetZ(m.r[i]) << "\t";
		os << XMVectorGetW(m.r[i]);
		os << endl;
	}

	return os;
}

int main() {
	XMMATRIX scale = XMMatrixScaling(2, 2, 2);
	XMMATRIX xRotation = XMMatrixRotationX(XM_PI);

	XMVECTOR n = XMVectorSet(1, 0, 0, 0);
	XMMATRIX rotation = XMMatrixRotationAxis(n, 45);

	XMMATRIX translation = XMMatrixTranslation(1, 1, 1);
	XMMATRIX translation2 = XMMatrixTranslationFromVector(n);


	XMVECTOR v = XMVector3TransformCoord(n, scale);
	XMFLOAT3 vF;
	XMStoreFloat3(&vF, v);

	XMVECTOR u = XMVector3TransformNormal(n, translation);
	XMFLOAT3 uF;
	XMStoreFloat3(&uF, u);

	cout << v << endl;
	cout << u << endl;
}
