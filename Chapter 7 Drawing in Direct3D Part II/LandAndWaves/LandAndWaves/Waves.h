#pragma once

#include <vector>
#include <DirectXMath.h>

class Waves {
public:
	Waves(int m, int n, float dx, float dt, float speed, float damping);
	Waves(const Waves& rhs) = delete;
	Waves& operator=(const Waves& rhs) = delete;
	~Waves();

	int RowCount() const;
	int ColumnCount() const;
	int VertexCount() const;
	int TriangleCount() const;
	float Width() const;
	float Depth() const;

	const DirectX::XMFLOAT3& Position(int i) const { return _currSolution[i]; }
	const DirectX::XMFLOAT3& Normal(int i) const { return _normals[i]; }
	const DirectX::XMFLOAT3& TangentX(int i) const { return _tangentX[i]; }

	void Update(float dt);
	void Disturb(int i, int j, float magnitude);

private:
	int _numRows = 0;
	int _numCols = 0;

	int _vertexCount = 0;
	int _triangleCount = 0;

	// Simulation constants we can precompute.
	float _k1 = 0.0f;
	float _k2 = 0.0f;
	float _k3 = 0.0f;

	float _timeStep = 0.0f;
	float _spatialStep = 0.0f;

	std::vector<DirectX::XMFLOAT3> _prevSolution;
	std::vector<DirectX::XMFLOAT3> _currSolution;
	std::vector<DirectX::XMFLOAT3> _normals;
	std::vector<DirectX::XMFLOAT3> _tangentX;
};
