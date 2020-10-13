#include "GeometryGenerator.h"
#include <algorithm>

using namespace DirectX;

GeometryGenerator::MeshData GeometryGenerator::CreateCylinder (
	float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount) {
	MeshData meshData;

	//
	// 構建堆疊層
	//

	// 每一層的高度
	float stackHeight = height / stackCount;

	// 計算從下至上遍歷每個相判分層時所需的半徑增量
	float radiusStep = (topRadius - bottomRadius) / stackCount;

	// 環數
	uint32 ringCount = stackCount + 1;

	// 環上的每個頂點的角度的增量
	float dTheta = 2.0f * XM_PI / sliceCount;

	for (uint32 i = 0; i < ringCount; ++i) {
		float y = -0.5f * height + i * stackHeight;
		float r = bottomRadius + i * radiusStep;

		for (uint32 j = 0; j <= sliceCount; ++j) {
			Vertex vertex;

			float c = cosf (j * dTheta);
			float s = sinf (j * dTheta);

			vertex.Position = XMFLOAT3 (r * c, y, r * s);

			vertex.TexC.x = (float)j / sliceCount;
			vertex.TexC.y = 1.0f - (float)i / stackCount;

			// 我們需要兩個切線向量來求出該頂點的法線向量
			// 我們可以在頂面和底面取兩點來求出一條切線
			// 其中r0表示底面半徑r1表示頂點半徑
			// 底面點: (r0 * c,  h/2, r0 * s)
			// 頂面點: (r1 * c, -h/2, r1 * s)
			// 切線 = 底面點 - 頂面點 (頂面點向底面點向量)
			// 副切線 = (c * (r0 - r1), -h, s * (r0 - r1))
			// dr = r0 - r1
			// bitangent (dr * c, -h, dr * s)

			// 此為單位長度
			vertex.TangentU = XMFLOAT3 (-s, 0.0f, c);

			float dr = bottomRadius - topRadius;
			XMFLOAT3 bitangent (dr * c, -height, dr * s);

			XMVECTOR T = XMLoadFloat3 (&vertex.TangentU);
			XMVECTOR B = XMLoadFloat3 (&bitangent);
			XMVECTOR N = XMVector3Normalize (XMVector3Cross (T, B));
			XMStoreFloat3 (&vertex.Normal, N);

			meshData.Vertices.push_back (vertex);
		}
	}

	// +1是希望讓每環的第一個頂點和最後一個頂點重合,這是因為它們的紋理坐標並不相同
	uint32 ringVertexCount = sliceCount + 1;

	// 計算每個側面塊中三角形的索引
	for (uint32 i = 0; i < stackCount; i++) {
		for (uint32 j = 0; j < sliceCount; ++j) {
			meshData.Indices32.push_back (i * ringVertexCount + j);
			meshData.Indices32.push_back ((i + 1) * ringVertexCount + j);
			meshData.Indices32.push_back ((i + 1) * ringVertexCount + j + 1);

			meshData.Indices32.push_back (i * ringVertexCount + j);
			meshData.Indices32.push_back ((i + 1) * ringVertexCount + j + 1);
			meshData.Indices32.push_back (i * ringVertexCount + j + 1);
		}
	}

	BuildCylinderTopCap (bottomRadius, topRadius, height, sliceCount, stackCount, meshData);
	BuildCylinderBottomCap (bottomRadius, topRadius, height, sliceCount, stackCount, meshData);

	return meshData;
}

void GeometryGenerator::BuildCylinderTopCap (
	float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount, MeshData& meshData) {
	uint32 baseIndex = (uint32)meshData.Vertices.size ();

	float y = 0.5f * height;
	float dTheta = 2.0f * XM_PI / sliceCount;

	// 使圓台端面環上的首尾頂點重合, 因為這兩個頂點的紋理坐標和法線是不同的
	for (uint32 i = 0; i <= sliceCount; ++i) {
		float x = topRadius * cosf (i * dTheta);
		float z = topRadius * sinf (i * dTheta);

		// 根據圓台的高度使頂面紋理坐標的範圍按比例縮小
		float u = x / height + 0.5f;
		float v = z / height + 0.5f;

		meshData.Vertices.emplace_back (Vertex (x, y, z, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, u, v));
	}

	// 頂面的中心頂點
	meshData.Vertices.emplace_back (Vertex (0.0f, y, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f, 0.5f));

	// 中心頂點的索引值
	uint32 centerIndex = (uint32)meshData.Vertices.size () - 1;

	for (uint32 i = 0; i < sliceCount; ++i) {
		meshData.Indices32.push_back (centerIndex);
		meshData.Indices32.push_back (baseIndex + i + 1);
		meshData.Indices32.push_back (baseIndex + i);
	}
}

void GeometryGenerator::BuildCylinderBottomCap (
	float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount, MeshData& meshData) {
	uint32 baseIndex = (uint32)meshData.Vertices.size ();

	float y = -0.5f * height;
	float dTheta = 2.0f * XM_PI / sliceCount;

	// 使圓台端面環上的首尾頂點重合, 因為這兩個頂點的紋理坐標和法線是不同的
	for (uint32 i = 0; i <= sliceCount; ++i) {
		float x = bottomRadius * cosf (i * dTheta);
		float z = bottomRadius * sinf (i * dTheta);

		// 根據圓台的高度使頂面紋理坐標的範圍按比例縮小
		float u = x / height + 0.5f;
		float v = z / height + 0.5f;

		meshData.Vertices.emplace_back (Vertex (x, y, z, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, u, v));
	}

	// 底面的中心頂點
	meshData.Vertices.emplace_back (Vertex (0.0f, y, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f, 0.5f));

	// 中心頂點的索引值
	uint32 centerIndex = (uint32)meshData.Vertices.size () - 1;

	for (uint32 i = 0; i < sliceCount; ++i) {
		meshData.Indices32.push_back (centerIndex);
		meshData.Indices32.push_back (baseIndex + i);
		meshData.Indices32.push_back (baseIndex + i + 1);
	}
}

GeometryGenerator::MeshData GeometryGenerator::CreateSphere (float radius, uint32 sliceCount, uint32 stackCount) {
	MeshData meshData;

	Vertex topVertex (0.0f, radius, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	Vertex bottomVertex (0.0f, -radius, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);

	meshData.Vertices.push_back (topVertex);

	float phiStep = XM_PI / stackCount;
	float thetaStep = 2.0f * XM_PI / sliceCount;

	for (uint32 i = 1; i < stackCount; ++i) {
		float phi = i * phiStep;

		// Vertices of ring.
		for (uint32 j = 0; j <= sliceCount; ++j) {
			float theta = j * thetaStep;

			Vertex v;

			// spherical to cartesian
			v.Position.x = radius * sinf (phi) * cosf (theta);
			v.Position.y = radius * cosf (phi);
			v.Position.z = radius * sinf (phi) * sinf (theta);

			// Partial derivative of P with respect to theta
			v.TangentU.x = -radius * sinf (phi) * sinf (theta);
			v.TangentU.y = 0.0f;
			v.TangentU.z = radius * sinf (phi) * cosf (theta);

			XMVECTOR T = XMLoadFloat3 (&v.TangentU);
			XMStoreFloat3 (&v.TangentU, XMVector3Normalize (T));

			XMVECTOR p = XMLoadFloat3 (&v.Position);
			XMStoreFloat3 (&v.Normal, XMVector3Normalize (p));

			v.TexC.x = theta / XM_2PI;
			v.TexC.y = phi / XM_PI;

			meshData.Vertices.push_back (v);
		}
	}

	meshData.Vertices.push_back (bottomVertex);

	// Compute indices for Top stack.
	for (uint32 i = 1; i <= sliceCount; ++i) {
		meshData.Indices32.push_back (0);
		meshData.Indices32.push_back (i + 1);
		meshData.Indices32.push_back (i);
	}

	// Compute indices for inner stacks
	uint32 baseIndex = 1;
	uint32 ringVertexCount = sliceCount + 1;
	for (uint32 i = 0; i < stackCount - 2; ++i) {
		for (uint32 j = 0; j < sliceCount; ++j) {
			meshData.Indices32.push_back (baseIndex + i * ringVertexCount + j);
			meshData.Indices32.push_back (baseIndex + i * ringVertexCount + j + 1);
			meshData.Indices32.push_back (baseIndex + (i + 1) * ringVertexCount + j);

			meshData.Indices32.push_back (baseIndex + (i + 1) * ringVertexCount + j);
			meshData.Indices32.push_back (baseIndex + i * ringVertexCount + j + 1);
			meshData.Indices32.push_back (baseIndex + (i + 1) * ringVertexCount + j + 1);
		}
	}

	// Compute indices for bottom stack.
	uint32 southPoleIndex = (uint32)meshData.Vertices.size () - 1;
	baseIndex = southPoleIndex - ringVertexCount;
	for (uint32 i = 0; i <= sliceCount; ++i) {
		meshData.Indices32.push_back (southPoleIndex);
		meshData.Indices32.push_back (baseIndex + i);
		meshData.Indices32.push_back (baseIndex + i + 1);
	}


	return meshData;
}

GeometryGenerator::MeshData GeometryGenerator::CreateGeosphere (float radius, uint32 numSubdivisions) {
	MeshData meshData;

	// 確定細分的次數
	numSubdivisions = std::min<uint32> (numSubdivisions, 6u);

	// 通過對一個正二十面體進行曲面細分來逼近一個球體

	const float x = 0.525731f;
	const float z = 0.850651f;

	XMFLOAT3 pos[12] = {
		XMFLOAT3 (-x, 0.0f, z), XMFLOAT3 (x, 0.0f, z),
		XMFLOAT3 (-x, 0.0f, -z), XMFLOAT3 (x, 0.0f, -z),
		XMFLOAT3 (0.0f, z, x), XMFLOAT3 (0.0f, z, -x),
		XMFLOAT3 (0.0f, -z, x), XMFLOAT3 (0.0f, -z, -x),
		XMFLOAT3 (z, x, 0.0f), XMFLOAT3 (-z, x, 0.0f),
		XMFLOAT3 (z, -x, 0.0f), XMFLOAT3 (-z, -x, 0.0f)
	};

	uint32 k[60] = {
		1, 4, 0, 
		4, 9, 0, 
		4, 5, 9, 
		8, 5, 4, 
		1, 8, 4,
		1, 10, 8, 
		10, 3, 8, 
		8, 3, 5, 
		3, 2, 5, 
		3, 7, 2,
		3, 10, 7, 
		10, 6, 7, 
		6, 11, 7, 
		6, 0, 11, 
		6, 1, 0,
		10, 1, 6, 
		11, 0, 9, 
		2, 11, 9, 
		5, 2, 9, 
		11, 2, 7
	};

	meshData.Vertices.resize (12);
	meshData.Indices32.assign (&k[0], &k[60]);

	for (uint32 i = 0; i < 12; ++i)
		meshData.Vertices[i].Position = pos[i];

	for (uint32 i = 0; i < numSubdivisions; ++i)
		Subdivide (meshData);

	// 將每一個頂點都投影到球面, 並推導其對應的紋理坐標
	for (uint32 i = 0; i < meshData.Vertices.size (); ++i) {
		// 投影到單位球面上
		XMVECTOR n = XMVector3Normalize (XMLoadFloat3 (&meshData.Vertices[i].Position));

		// 投射到球面上
		XMVECTOR p = radius * n;

		XMStoreFloat3 (&meshData.Vertices[i].Position, p);
		XMStoreFloat3 (&meshData.Vertices[i].Normal, n);

		// 根據球面坐標推導出紋理坐標
		float theta = atan2f (meshData.Vertices[i].Position.z, meshData.Vertices[i].Position.x);

		// 將theta限制在[0, 360]區間內
		if (theta < 0.0f)
			theta += XM_2PI;
		
		float phi = acosf (meshData.Vertices[i].Position.y / radius);

		meshData.Vertices[i].TexC.x = theta / XM_2PI;
		meshData.Vertices[i].TexC.y = phi / XM_PI;

		// 求出P關於theta的偏導數
		meshData.Vertices[i].TangentU.x = -radius * sinf (phi) * sinf (theta);
		meshData.Vertices[i].TangentU.y = 0.0f;
		meshData.Vertices[i].TangentU.z = radius * sinf (phi) * cosf (theta);

		XMVECTOR T = XMLoadFloat3 (&meshData.Vertices[i].TangentU);
		XMStoreFloat3 (&meshData.Vertices[i].TangentU, XMVector3Normalize (T));
	}

	return meshData;
}

void GeometryGenerator::Subdivide (MeshData& meshData) {
	// Save a copy of the input geometry.
	MeshData inputCopy = meshData;

	meshData.Vertices.resize (0);
	meshData.Indices32.resize (0);

	//       v1
	//       *
	//      / \
	//     /   \
	//  m0*-----*m1
	//   / \   / \
	//  /   \ /   \
	// *-----*-----*
	// v0    m2     v2

	uint32 numTris = (uint32)inputCopy.Indices32.size () / 3;
	for (uint32 i = 0; i < numTris; ++i) {
		Vertex v0 = inputCopy.Vertices[inputCopy.Indices32[i * 3 + 0]];
		Vertex v1 = inputCopy.Vertices[inputCopy.Indices32[i * 3 + 1]];
		Vertex v2 = inputCopy.Vertices[inputCopy.Indices32[i * 3 + 2]];

		//
		// Generate the midpoints.
		//

		Vertex m0 = MidPoint (v0, v1);
		Vertex m1 = MidPoint (v1, v2);
		Vertex m2 = MidPoint (v0, v2);

		//
		// Add new geometry.
		//

		meshData.Vertices.push_back (v0); // 0
		meshData.Vertices.push_back (v1); // 1
		meshData.Vertices.push_back (v2); // 2
		meshData.Vertices.push_back (m0); // 3
		meshData.Vertices.push_back (m1); // 4
		meshData.Vertices.push_back (m2); // 5

		meshData.Indices32.push_back (i * 6 + 0);
		meshData.Indices32.push_back (i * 6 + 3);
		meshData.Indices32.push_back (i * 6 + 5);

		meshData.Indices32.push_back (i * 6 + 3);
		meshData.Indices32.push_back (i * 6 + 4);
		meshData.Indices32.push_back (i * 6 + 5);

		meshData.Indices32.push_back (i * 6 + 5);
		meshData.Indices32.push_back (i * 6 + 4);
		meshData.Indices32.push_back (i * 6 + 2);

		meshData.Indices32.push_back (i * 6 + 3);
		meshData.Indices32.push_back (i * 6 + 1);
		meshData.Indices32.push_back (i * 6 + 4);
	}
}

GeometryGenerator::Vertex GeometryGenerator::MidPoint (const Vertex& v0, const Vertex& v1) {
	XMVECTOR p0 = XMLoadFloat3 (&v0.Position);
	XMVECTOR p1 = XMLoadFloat3 (&v1.Position);

	XMVECTOR n0 = XMLoadFloat3 (&v0.Normal);
	XMVECTOR n1 = XMLoadFloat3 (&v1.Normal);

	XMVECTOR tan0 = XMLoadFloat3 (&v0.TangentU);
	XMVECTOR tan1 = XMLoadFloat3 (&v1.TangentU);

	XMVECTOR tex0 = XMLoadFloat2 (&v0.TexC);
	XMVECTOR tex1 = XMLoadFloat2 (&v1.TexC);

	// Compute the midpoints of all the attributes.  Vectors need to be normalized
	// since linear interpolating can make them not unit length.  
	XMVECTOR pos = 0.5f * (p0 + p1);
	XMVECTOR normal = XMVector3Normalize (0.5f * (n0 + n1));
	XMVECTOR tangent = XMVector3Normalize (0.5f * (tan0 + tan1));
	XMVECTOR tex = 0.5f * (tex0 + tex1);

	Vertex v;
	XMStoreFloat3 (&v.Position, pos);
	XMStoreFloat3 (&v.Normal, normal);
	XMStoreFloat3 (&v.TangentU, tangent);
	XMStoreFloat2 (&v.TexC, tex);

	return v;
}

GeometryGenerator::MeshData GeometryGenerator::CreateBox (float width, float height, float depth, uint32 numSubdivisions) {
	MeshData meshData;

	//
	// Create vertices.
	//

	// 6面,24頂點(每面4頂點)	
	Vertex v[24];

	float w = 0.5f * width;
	float h = 0.5f * height;
	float d = 0.5f * depth;

	// Fill in the front face vertex data.
	v[0] = Vertex (-w, -h, -d, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);	// Lower left
	v[1] = Vertex (-w, h, -d, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);		// Top left
	v[2] = Vertex (w, h, -d, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);		// Top right
	v[3] = Vertex (w, -h, -d, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);		// Lower right

	// Fill in the back face vertex data.
	v[4] = Vertex (w, -h, d, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	v[5] = Vertex (w, h, d, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	v[6] = Vertex (-w, h, d, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
	v[7] = Vertex (-w, -h, d, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

	// Fill in the top face vertex data.
	v[8] = Vertex (-w, h, -d, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	v[9] = Vertex (-w, h, d, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	v[10] = Vertex (w, h, d, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
	v[11] = Vertex (w, h, -d, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

	// Fill in the bottom face vertex data.
	v[12] = Vertex (w, -h, -d, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	v[13] = Vertex (w, -h, d, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	v[14] = Vertex (-w, -h, d, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
	v[15] = Vertex (-w, -h, -d, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

	// Fill in the left face vertex data.
	v[16] = Vertex (-w, -h, d, -1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f);
	v[17] = Vertex (-w, h, d, -1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f);
	v[18] = Vertex (-w, h, -d, -1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f);
	v[19] = Vertex (-w, -h, -d, -1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f);

	// Fill in the right face vertex data.
	v[20] = Vertex (w, -h, -d, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
	v[21] = Vertex (w, h, -d, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
	v[22] = Vertex (w, h, d, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
	v[23] = Vertex (w, -h, d, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);

	meshData.Vertices.assign (&v[0], &v[24]);

	//
	// Create indices.
	//

	// 每面兩個三角形,即為每面6頂點,一共36個索引
	uint32 i[36];

	// Fill in the front face index data
	i[0] = 0; i[1] = 1; i[2] = 2;
	i[3] = 0; i[4] = 2; i[5] = 3;

	// Fill in the bottom face index data
	i[6] = 4; i[7] = 5; i[8] = 6;
	i[9] = 4; i[10] = 6; i[11] = 7;

	// Fill in the top face index data
	i[12] = 8; i[13] = 9; i[14] = 10;
	i[15] = 8; i[16] = 10; i[17] = 11;

	// Fill in the bottom face index data
	i[18] = 12; i[19] = 13; i[20] = 14;
	i[21] = 12; i[22] = 14; i[23] = 15;

	// Fill in the left face index data
	i[24] = 16; i[25] = 17; i[26] = 18;
	i[27] = 16; i[28] = 18; i[29] = 19;

	// Fill in the right face index data
	i[30] = 20; i[31] = 21; i[32] = 22;
	i[33] = 20; i[34] = 22; i[35] = 23;

	meshData.Indices32.assign (&i[0], &i[36]);

	// 給細分設定一個上限
	numSubdivisions = std::min<uint32> (numSubdivisions, 6u);

	for (uint32 i = 0; i < numSubdivisions; ++i)
		Subdivide (meshData);

	return meshData;
}

//
// m : row
// n : column
//
GeometryGenerator::MeshData GeometryGenerator::CreateGrid (float width, float depth, uint32 m, uint32 n) {
	MeshData meshData;

	uint32 vertexCount = m * n;
	uint32 faceCount = (m - 1) * (n - 1) * 2;

	//
	// Create vertices.
	//

	float halfWidth = 0.5f * width;
	float halfDepth = 0.5f * depth;

	float dx = width / (n - 1);
	float dz = depth / (m - 1);

	float du = 1.0f / (n - 1);
	float dv = 1.0f / (m - 1);

	meshData.Vertices.resize (vertexCount);
	for (uint32 i = 0; i < m; ++i) {
		float z = halfDepth - i * dz;
		for (uint32 j = 0; j < n; ++j) {
			float x = -halfWidth + j * dx;

			// 第n行,第j列
			int index = i * n + j;
			meshData.Vertices[index].Position = XMFLOAT3 (x, 0.0f, z);
			meshData.Vertices[index].Normal = XMFLOAT3 (0.0f, 1.0f, 0.0f);
			meshData.Vertices[index].TangentU = XMFLOAT3 (1.0f, 0.0f, 0.0f);

			// Stretch texture over grid.
			meshData.Vertices[index].TexC.x = j * du;
			meshData.Vertices[index].TexC.y = i * dv;
		}
	}

	//
	// Create indices
	//

	meshData.Indices32.resize (faceCount * 3);	// 3 indices per face

	// Iterate over each quad and compute indices.
	uint32 k = 0;
	for (uint32 i = 0; i < m - 1; ++i) {
		for (uint32 j = 0; j < n - 1; ++j) {
			meshData.Indices32[k] = i * n + j;					// top left
			meshData.Indices32[k + 1] = i * n + j + 1;			// top right
			meshData.Indices32[k + 2] = (i + 1) * n + j;		// lower left

			meshData.Indices32[k + 3] = (i + 1) * n + j;		// lower left
			meshData.Indices32[k + 4] = i * n + j + 1;			// top right
			meshData.Indices32[k + 5] = (i + 1) * n + j + 1;	// lower right

			k += 6;	// next quad
		}
	}

	return meshData;
}

//
// w : x offset
// h : y offset
//
GeometryGenerator::MeshData GeometryGenerator::CreateQuad (float x, float y, float w, float h, float depth) {
	MeshData meshData;

	meshData.Vertices.resize (4);
	meshData.Indices32.resize (6);

	// Position coordinates specified in NDC space.
	meshData.Vertices[0] = Vertex (x, y - h, depth, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	meshData.Vertices[1] = Vertex (x, y, depth, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	meshData.Vertices[2] = Vertex (x + w, y, depth, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
	meshData.Vertices[3] = Vertex (x + w, y - h, depth, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

	meshData.Indices32[0] = 0;
	meshData.Indices32[1] = 1;
	meshData.Indices32[2] = 2;

	meshData.Indices32[3] = 0;
	meshData.Indices32[4] = 2;
	meshData.Indices32[5] = 3;

	return meshData;
}