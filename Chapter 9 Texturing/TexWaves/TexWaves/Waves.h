#pragma once

#include <vector>
#include <DirectXMath.h>

class Waves {
public:
    Waves (int m, int n, float dx, float dt, float speed, float damping);
    Waves (const Waves& rhs) = delete;
    Waves& operator=(const Waves& rhs) = delete;
    ~Waves ();

    int RowCount ()const;
    int ColumnCount ()const;
    int VertexCount ()const;
    int TriangleCount ()const;
    float Width ()const;
    float Depth ()const;

    // Returns the solution at the ith grid point.
    const DirectX::XMFLOAT3& Position (int i)const { return m_CurrSolution[i]; }

    // Returns the solution normal at the ith grid point.
    const DirectX::XMFLOAT3& Normal (int i)const { return m_Normals[i]; }

    // Returns the unit tangent vector at the ith grid point in the local x-axis direction.
    const DirectX::XMFLOAT3& TangentX (int i)const { return m_TangentX[i]; }

    void Update (float dt);
    void Disturb (int i, int j, float magnitude);

private:
    int m_NumRows = 0;
    int m_NumCols = 0;

    int m_VertexCount = 0;
    int m_TriangleCount = 0;

    // Simulation constants we can precompute.
    float m_K1 = 0.0f;
    float m_K2 = 0.0f;
    float m_K3 = 0.0f;

    float m_TimeStep = 0.0f;
    float m_SpatialStep = 0.0f;

    std::vector<DirectX::XMFLOAT3> m_PrevSolution;
    std::vector<DirectX::XMFLOAT3> m_CurrSolution;
    std::vector<DirectX::XMFLOAT3> m_Normals;
    std::vector<DirectX::XMFLOAT3> m_TangentX;
};

