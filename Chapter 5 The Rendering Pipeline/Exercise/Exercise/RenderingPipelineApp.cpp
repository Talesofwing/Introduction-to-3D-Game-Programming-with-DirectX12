#include <array>
#include <DirectXMath.h>

using namespace DirectX;

struct Vertex {
	XMFLOAT3 Pos;
};

void BuildPyramidGeometry() {
	std::array<Vertex, 8> vertices = {
		Vertex({XMFLOAT3(0.0f,  2.0f,  0.0f)}),	// top
		Vertex({XMFLOAT3(-1.0f,  0.0f, -1.0f)}),
		Vertex({XMFLOAT3(-1.0f,  0.0f,  1.0f)}),
		Vertex({XMFLOAT3(1.0f,  0.0f,  1.0f)}),
		Vertex({XMFLOAT3(1.0f,  0.0f, -1.0f)})
	};

	std::array<std::uint16_t, 18> indices = {
		1, 0, 4,
		2, 0, 1,
		3, 0, 2,
		4, 0, 3,
		1, 2, 4,
		2, 3, 4
	};
}

void BuildCombinationVertexAndIndex() {
	std::array<Vertex, 13> vertices = {
		// parallelogram
		Vertex({XMFLOAT3(-1.0f, -1.0f, 0.0f)}),
		Vertex({XMFLOAT3(-0.5f,  1.0f, 0.0f)}),
		Vertex({XMFLOAT3(1.0f,  1.0f, 0.0f)}),
		Vertex({XMFLOAT3(0.5f, -1.0f, 0.0f)}),
		// polygon
		Vertex({XMFLOAT3(0.0f,  0.0f, 0.0f)}),	// center
		Vertex({XMFLOAT3(2.0f,  0.0f, 0.0f)}),	// right
		Vertex({XMFLOAT3(1.0f, -0.5f, 0.0f)}),
		Vertex({XMFLOAT3(0.0f, -1.0f, 0.0f)}),	// bottom
		Vertex({XMFLOAT3(-1.0f, -0.5f, 0.0f)}),
		Vertex({XMFLOAT3(-2.0f,  0.0f, 0.0f)}),	// left
		Vertex({XMFLOAT3(-1.0f,  0.5f, 0.0f)}),
		Vertex({XMFLOAT3(0.0f,  1.0f, 0.0f)}),	// top
		Vertex({XMFLOAT3(1.0f,  0.5f, 0.0f)}),
	};

	std::array<std::uint16_t, 27> indices = {
		// parallelogram
		0, 1, 2,
		0, 2, 3,
		// polygon
		// all + 4 (parallelogram has four vertex)
		// so polygon is 4 first
		0 + 4, 1 + 4, 2 + 4,
		0 + 4, 2 + 4, 3 + 4,
		0 + 4, 3 + 4, 4 + 4,
		0 + 4, 4 + 4, 5 + 4,
		0 + 4, 5 + 4, 6 + 4,
		0 + 4, 6 + 4, 7 + 4,
		0 + 4, 7 + 4, 8 + 4
	};
}

int main() {

}
