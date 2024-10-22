#pragma once

#include <windows.h>
#include <wrl.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <string>
#include <vector>
#include <array>
#include <DirectXColors.h>
#include <DirectXCollision.h>
#include <unordered_map>
#include <memory>
#include <fstream>

#include "d3dx12.h"
#include "MathHelper.h"

extern const int g_NumFrameResources;

class D3DUtil {
public:
	static Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmdList,
		const void* initData,
		UINT64 byteSize,
		Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);

	static UINT CalcConstantBufferByteSize(UINT byteSize)
	{
		return (byteSize + 255) & ~255;
	}

	static Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(const std::wstring& filename,
														  const D3D_SHADER_MACRO* defines,
														  const std::string& entrypoint,
														  const std::string& target);

	static Microsoft::WRL::ComPtr<ID3DBlob> LoadBinary(const std::wstring& filename);
};

class DxException {
public:
	DxException() = default;
	DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber);

	std::wstring ToString() const;

	HRESULT ErrorCode = S_OK;
	std::wstring FunctionName;
	std::wstring Filename;
	int LineNumber = -1;
};

// 定義MeshGeometry中存儲的單個幾何體
// 此結構體適用於將多個幾何體數據存於一個頂點緩沖區和一個索引緩沖區的情況
// 它提供了對存於頂點緩沖區和索引緩沖區中的單個幾何體進行繪制所需的數據和偏移量
struct SubmeshGeometry {
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	INT BaseVertexLocation = 0;

	// 通過此子網格來定當前SubMeshGeometry結構體中所存幾何體的包圍盒
	DirectX::BoundingBox Bounds;
};

struct MeshGeometry {
	// 指定此幾何體網格集合的名稱, 這樣我們就能根據此名找到它
	std::string Name;

	// 系統內存中的副本。由於頂點/索引可以是泛型格式,所以用Blob類型來表示
	// 待用戶在使用時可將其轉換為適當的類型
	Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

	// 與緩沖區相關的數據
	UINT VertexByteStride = 0;
	UINT VertexBufferByteSize = 0;
	DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
	UINT IndexBufferByteSize = 0;

	// 一個MeshGeometry結構體能夠存儲一組頂點/索引緩沖區中的多個幾何體
	// 若利用下列器來定子網格幾何體, 我們就能單獨地繪制出其中的子網格 (單個幾何體)
	std::unordered_map<std::string, SubmeshGeometry> DrawArgs;

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const
	{
		D3D12_VERTEX_BUFFER_VIEW vbv;
		vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
		vbv.StrideInBytes = VertexByteStride;
		vbv.SizeInBytes = VertexBufferByteSize;
		return vbv;
	}

	D3D12_INDEX_BUFFER_VIEW IndexBufferView() const
	{
		D3D12_INDEX_BUFFER_VIEW ibv;
		ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
		ibv.Format = IndexFormat;
		ibv.SizeInBytes = IndexBufferByteSize;
		return ibv;
	}

	// 待數據上傳至GPU後, 釋放內存
	void DisposeUploaders()
	{
		VertexBufferUploader = nullptr;
		IndexBufferUploader = nullptr;
	}
};

struct MaterialConstants {
	DirectX::XMFLOAT4 DiffuseAlbedo = {1.0f, 1.0f, 1.0f, 1.0f};
	DirectX::XMFLOAT3 FresnelR0 = {0.01f, 0.01f, 0.01f};
	float Roughness = 0.25f;

	// Used in texture mapping.
	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};

struct Material {
	// 便於查找材質的唯一對應名稱
	std::string Name;

	// 材質的常量綬沖區索引
	int MatCBIndex = -1;

	// 漫反射紋理在SRV堆中的索引
	int DiffuseSrvHeapIndex = -1;

	// 法線紋理在SRV堆中的索引
	int NormalSrvHeapIndex = -1;

	int NumFrameDirty = g_NumFrameResources;

	// 用於著色的材質常量綬緩沖區數據
	DirectX::XMFLOAT4 DiffuseAlbedo = {1.0f, 1.0f, 1.0f, 1.0f};		// 漫反射反照率
	DirectX::XMFLOAT3 FresnelR0 = {0.01f, 0.01f, 0.01f};			// 材質屬性 (折射率)
	float Roughness = 0.25f;										// 粗糙度 [0, 1] 0為理想光滑表面
	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};

struct Light {
	DirectX::XMFLOAT3 Strength = {0.5f, 0.5f, 0.5f};
	float FalloffStart = 1.0f;							// point/spot light only
	DirectX::XMFLOAT3 Direction = {0.0f, -1.0f, 0.0f};  // directional/spot light only
	float FalloffEnd = 10.0f;							// point/spot light only
	DirectX::XMFLOAT3 Position = {0.0f, 0.0f, 0.0f};	// point/spot light only
	float SpotPower = 64.0f;							// spot light only
};

#define MaxLights 16

struct Texture {
	// 便於查找材質所用的唯一名稱
	std::string Name;

	std::wstring Filename;

	Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
};

inline std::wstring AnsiToWString(const std::string& str)
{
	WCHAR buffer[512];
	MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
	return std::wstring(buffer);
}

#ifndef ThrowIfFailed
#define ThrowIfFailed(x) {									\
	HRESULT hr__ = x;										\
	std::wstring wfn = AnsiToWString (__FILE__);			\
	if (FAILED (hr__)) {									\
		throw DxException (hr__, L#x, wfn, __LINE__);		\
	}														\
}
#endif

#ifndef ReleaseCom
#define ReleaseCom(x) { if (x) { x->Release(); x = nullptr; } }
#endif