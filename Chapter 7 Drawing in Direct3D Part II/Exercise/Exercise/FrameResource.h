#pragma once

#include "d3dUtil.h"
#include "MathHelper.h"
#include "UploadBuffer.h"

struct ObjectConstants {
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4 ();
};

struct PassConstants {
	DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4 ();
	DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4 ();
	DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4 ();
	DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4 ();
	DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4 ();
	DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4 ();
	DirectX::XMFLOAT3 EyePosW = {0.0f, 0.0f, 0.0f};
	float cbPerObjectPad1 = 0.0f;
	DirectX::XMFLOAT2 RenderTargetSize = {0.0f, 0.0f};
	DirectX::XMFLOAT2 InvRenderTargetSize = {0.0f, 0.0f};
	float NearZ = 0.0f;
	float FarZ = 0.0f;
	float TotalTime = 0.0f;
	float DeltaTime = 0.0f;
};

struct Vertex {
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT4 Color;
	DirectX::XMFLOAT3 Normal;
};

// 存有CPU為構建每幀命令列表所需的資源
// 其中的數據將依程序而異,這取決於實際繪制所需的資源
struct FrameResource {
public:
	FrameResource (ID3D12Device* device, UINT passCount, UINT objectCount);
	FrameResource (const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource ();

	// 在GPU處理完與此命令分配器相關的命令之前, 都不能對它進行重置
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	// 在GPU執行完引用此常量緩沖區的命令之前,我們不能對他進行更新
	// 因此每一幀都要有它們自己的常量緩沖區
	std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;

	// 通過Fence值將命令標記到此Fence點, 這使我們可以檢測到GPU是否還在使用這些幀資源
	UINT64 Fence = 0;
};

