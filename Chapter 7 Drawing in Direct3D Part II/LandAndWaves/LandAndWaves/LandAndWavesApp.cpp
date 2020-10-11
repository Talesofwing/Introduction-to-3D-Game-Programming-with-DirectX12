#include "../../../Common/D3DApp.h"
#include "../../../Common/MathHelper.h"
#include "../../../Common/UploadBuffer.h"
#include "../../../Common/GeometryGenerator.h"
#include "FrameResource.h"
#include "Waves.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

const int g_NumFrameResources = 3;

struct RenderItem {
	RenderItem () = default;

	XMFLOAT4X4 World = MathHelper::Identity4x4 ();

	int NumFrameDirty = g_NumFrameResources;

	UINT ObjCBIndex = -1;

	MeshGeometry* Geo = nullptr;

	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};

enum class RenderLayer : int {
	Opaque = 0,
	Count
};

class LandAndWavesApp : public D3DApp {
public:
	LandAndWavesApp (HINSTANCE hInstance);
	LandAndWavesApp (const LandAndWavesApp& rhs) = delete;
	LandAndWavesApp& operator=(const LandAndWavesApp& rhs) = delete;
	~LandAndWavesApp ();

	virtual bool Initialize () override;

private:
	virtual void OnResize () override;
	virtual void Update (const GameTimer& gt) override;
	virtual void Draw (const GameTimer& gt) override;

	virtual void OnMouseDown (WPARAM btnState, int x, int y) override;
	virtual void OnMouseUp (WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove (WPARAM btnState, int x, int y) override;

	void OnKeyboardInput (const GameTimer& gt);
	void UpdateCamera (const GameTimer& gt);
	void UpdateObjectCBs (const GameTimer& gt);
	void UpdateMainPassCB (const GameTimer& gt);
	void UpdateWaves (const GameTimer& gt);

	void BuildRootSignature ();
	void BuildShadersAndInputLayout ();
	void BuildLandGeometry ();
	void BuildWavesGeometryBuffers ();
	void BuildPSOs ();
	void BuildFrameResources ();
	void BuildRenderItems ();
	void DrawRenderItems (ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	float GetHillsHeight (float x, float z)const;
	XMFLOAT3 GetHillsNormal (float x, float z)const;

private:
	std::vector<std::unique_ptr<FrameResource>> m_FrameResources;
	FrameResource* m_CurrFrameResource = nullptr;
	int m_CurrFrameResourceIndex = 0;

	ComPtr<ID3D12RootSignature> m_RootSignature = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> m_Geometries;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> m_Shaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> m_PSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> m_InputLayout;

	std::vector<std::unique_ptr<RenderItem>> m_AllRitems;

	std::vector<RenderItem*> m_RitemLayer[(int)RenderLayer::Count];

	RenderItem* m_WavesRitem = nullptr;
	std::unique_ptr<Waves> m_Waves;

	PassConstants m_MainPassCB;

	bool m_IsWireFrame = false;

	XMFLOAT3 m_Eye = {0.0f, 0.0f, 0.0f};
	XMFLOAT4X4 m_View = MathHelper::Identity4x4 ();
	XMFLOAT4X4 m_Proj = MathHelper::Identity4x4 ();

	float m_Theta = 1.5f * XM_PI;
	float m_Phi = XM_PIDIV2 - 0.1f;
	float m_Radius = 50.0f;

	POINT m_LastMousePos;
};

LandAndWavesApp::LandAndWavesApp (HINSTANCE hInstance) : D3DApp (hInstance) {
	m_MainWndCaption = L"Chapter - 7 - LandAndWaves";
}

LandAndWavesApp::~LandAndWavesApp () {
	if (m_Device != nullptr)
		FlushCommandQueue ();
}

void LandAndWavesApp::OnResize () {
	D3DApp::OnResize ();

	XMMATRIX P = XMMatrixPerspectiveFovLH (0.25f * MathHelper::PI, AspectRatio (), 1.0f, 1000.0f);
	XMStoreFloat4x4 (&m_Proj, P);
}

bool LandAndWavesApp::Initialize () {
	if (!D3DApp::Initialize ())
		return false;

	ThrowIfFailed (m_CmdList->Reset (m_CmdAllocator.Get (), nullptr));

	m_Waves = std::make_unique<Waves> (128, 128, 1.0f, 0.03f, 4.0f, 0.2f);

	BuildRootSignature ();
	BuildShadersAndInputLayout ();
	BuildLandGeometry ();
	BuildWavesGeometryBuffers ();
	BuildRenderItems ();
	BuildFrameResources ();
	BuildPSOs ();

	ThrowIfFailed (m_CmdList->Close ());
	ID3D12CommandList* cmdsLists[] = {m_CmdList.Get ()};
	m_CmdQueue->ExecuteCommandLists (_countof (cmdsLists), cmdsLists);

	FlushCommandQueue ();

	for (auto& geo : m_Geometries) {
		geo.second->DisposeUploaders ();
	}

	return true;
}

void LandAndWavesApp::BuildRootSignature () {
	CD3DX12_ROOT_PARAMETER slotRootParameter[2];

	slotRootParameter[0].InitAsConstantBufferView (0);
	slotRootParameter[1].InitAsConstantBufferView (1);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc (2, slotRootParameter, 0, nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature (&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf (), errorBlob.GetAddressOf ());

	if (errorBlob != nullptr) {
		OutputDebugStringA ((char*)errorBlob->GetBufferPointer ());
	}
	ThrowIfFailed (hr);

	ThrowIfFailed (m_Device->CreateRootSignature (
		0,
		serializedRootSig->GetBufferPointer (),
		serializedRootSig->GetBufferSize (),
		IID_PPV_ARGS (m_RootSignature.GetAddressOf ())
	));
}

void LandAndWavesApp::BuildShadersAndInputLayout () {
	m_Shaders["standardVS"] = D3DUtil::CompileShader (L"Shaders/color.hlsl", nullptr, "VS", "vs_5_0");
	m_Shaders["opaquePS"] = D3DUtil::CompileShader (L"Shaders/color.hlsl", nullptr, "PS", "ps_5_0");

	m_InputLayout = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
}

void LandAndWavesApp::BuildLandGeometry () {
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData land = geoGen.CreateGrid (160.0f, 160.0f, 50, 50);

	//
	// 獲取我們所需要的頂點元素, 並利用高度函數計算每個頂點的高度值
	// 另外,頂點的顏色要基於他們的高度而定
	// 所以,圖像中才會有看起來如沙質的沙灘,山腰處的植被以及山峰處的積雪
	//

	std::vector<Vertex> vertices (land.Vertices.size ());
	for (size_t i = 0; i < land.Vertices.size (); ++i) {
		auto& p = land.Vertices[i].Position;
		vertices[i].Pos = p;
		vertices[i].Pos.y = GetHillsHeight (p.x, p.z);

		// 基於頂點高度為它上色
		if (vertices[i].Pos.y < -10.0f) {
			// 沙灘色
			vertices[i].Color = XMFLOAT4 (1.0f, 0.96f, 0.62f, 1.0f);
		} else if (vertices[i].Pos.y < 5.0f) {
			// 淺黃綠色
			vertices[i].Color = XMFLOAT4 (0.48f, 0.77f, 0.46f, 1.0f);
		} else if (vertices[i].Pos.y < 12.0f) {
			// 深黃綠色
			vertices[i].Color = XMFLOAT4 (0.1f, 0.48f, 0.19f, 1.0f);
		} else if (vertices[i].Pos.y < 20.0f) {
			// 深粽色
			vertices[i].Color = XMFLOAT4 (0.45f, 0.39f, 0.34f, 1.0f);
		} else {
			// 白雪色
			vertices[i].Color = XMFLOAT4 (1.0f, 1.0f, 1.0f, 1.0f);
		}
	}

	const UINT vbByteSize = (UINT)vertices.size () * sizeof (Vertex);

	std::vector<std::uint16_t> indices = land.GetIndices16 ();
	const UINT ibByteSize = (UINT)indices.size () * sizeof (std::uint16_t);

	auto geo = std::make_unique<MeshGeometry> ();
	geo->Name = "land";

	ThrowIfFailed (D3DCreateBlob (vbByteSize, &geo->VertexBufferCPU));
	CopyMemory (geo->VertexBufferCPU->GetBufferPointer (), vertices.data (), vbByteSize);

	ThrowIfFailed (D3DCreateBlob (ibByteSize, &geo->IndexBufferCPU));
	CopyMemory (geo->IndexBufferCPU->GetBufferPointer (), vertices.data (), ibByteSize);

	geo->VertexBufferGPU = D3DUtil::CreateDefaultBuffer (m_Device.Get (), m_CmdList.Get (), vertices.data (), vbByteSize, geo->VertexBufferUploader);
	geo->IndexBufferGPU = D3DUtil::CreateDefaultBuffer (m_Device.Get (), m_CmdList.Get (), indices.data (), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof (Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size ();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["land"] = submesh;
	m_Geometries["land"] = std::move (geo);
}

void LandAndWavesApp::BuildWavesGeometryBuffers () {
	std::vector<std::uint16_t> indices (3 * m_Waves->TriangleCount ());	// 3 indices per face
	assert (m_Waves->VertexCount () < 0x0000ffff);

	// Iterate over each quad.
	int m = m_Waves->RowCount ();
	int n = m_Waves->ColumnCount ();
	int k = 0;
	for (int i = 0; i < m - 1; ++i) {
		for (int j = 0; j < n - 1; ++j) {
			indices[k] = i * n + j;
			indices[k + 1] = i * n + j + 1;
			indices[k + 2] = (i + 1) * n + j;

			indices[k + 3] = (i + 1) * n + j;
			indices[k + 4] = i * n + j + 1;
			indices[k + 5] = (i + 1) * n + j + 1;

			k += 6;
		}
	}

	UINT vbByteSize = m_Waves->VertexCount () * sizeof (Vertex);
	UINT ibByteSize = (UINT)indices.size () * sizeof (std::uint16_t);

	auto geo = std::make_unique<MeshGeometry> ();
	geo->Name = "water";

	// set dynamically.
	geo->VertexBufferCPU = nullptr;
	geo->VertexBufferGPU = nullptr;

	ThrowIfFailed (D3DCreateBlob (ibByteSize, &geo->IndexBufferCPU));
	CopyMemory (geo->IndexBufferCPU->GetBufferPointer (), indices.data (), ibByteSize);

	geo->IndexBufferGPU = D3DUtil::CreateDefaultBuffer (m_Device.Get (), m_CmdList.Get (), 
		indices.data (), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof (Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size ();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["water"] = submesh;

	m_Geometries["water"] = std::move (geo);
}

void LandAndWavesApp::BuildRenderItems () {
	auto wavesRitem = std::make_unique<RenderItem> ();
	wavesRitem->World = MathHelper::Identity4x4 ();
	wavesRitem->ObjCBIndex = 0;
	wavesRitem->Geo = m_Geometries["water"].get ();
	wavesRitem->PrimitiveType = D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wavesRitem->IndexCount = wavesRitem->Geo->DrawArgs["water"].IndexCount;
	wavesRitem->StartIndexLocation = wavesRitem->Geo->DrawArgs["water"].StartIndexLocation;
	wavesRitem->BaseVertexLocation = wavesRitem->Geo->DrawArgs["water"].BaseVertexLocation;

	m_WavesRitem = wavesRitem.get ();

	m_RitemLayer[(int)RenderLayer::Opaque].push_back (wavesRitem.get ());

	auto landRitem = std::make_unique<RenderItem> ();
	landRitem->World = MathHelper::Identity4x4 ();
	landRitem->ObjCBIndex = 0;
	landRitem->Geo = m_Geometries["land"].get ();
	landRitem->PrimitiveType = D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	landRitem->IndexCount = landRitem->Geo->DrawArgs["land"].IndexCount;
	landRitem->StartIndexLocation = landRitem->Geo->DrawArgs["land"].StartIndexLocation;
	landRitem->BaseVertexLocation = landRitem->Geo->DrawArgs["land"].BaseVertexLocation;

	m_RitemLayer[(int)RenderLayer::Opaque].push_back (landRitem.get ());

	m_AllRitems.push_back (std::move (wavesRitem));
	m_AllRitems.push_back (std::move (landRitem));
}

void LandAndWavesApp::BuildFrameResources () {
	for (int i = 0; i < g_NumFrameResources; i++)
		m_FrameResources.push_back (std::make_unique<FrameResource> (m_Device.Get (), 1, (UINT)m_AllRitems.size (), m_Waves->VertexCount ()));
}

void LandAndWavesApp::BuildPSOs () {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;
	ZeroMemory (&opaquePsoDesc, sizeof (D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	//
	// PSO for opaque objects.
	//
	opaquePsoDesc.InputLayout = {m_InputLayout.data (), (UINT)m_InputLayout.size ()};
	opaquePsoDesc.pRootSignature = m_RootSignature.Get ();
	opaquePsoDesc.VS = {
		m_Shaders["standardVS"]->GetBufferPointer (),
		m_Shaders["standardVS"]->GetBufferSize ()
	};
	opaquePsoDesc.PS = {
		m_Shaders["opaquePS"]->GetBufferPointer (),
		m_Shaders["opaquePS"]->GetBufferSize ()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC (D3D12_DEFAULT);
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC (D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC (D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
opaquePsoDesc.NumRenderTargets = 1;
opaquePsoDesc.RTVFormats[0] = m_BackBufferFormat;
opaquePsoDesc.SampleDesc.Count = m_4xMsaaState ? 4 : 1;
opaquePsoDesc.SampleDesc.Quality = m_4xMsaaState ? (m_4xMsaaQuality - 1) : 0;
opaquePsoDesc.DSVFormat = m_DepthStencilFormat;
ThrowIfFailed (m_Device->CreateGraphicsPipelineState (&opaquePsoDesc, IID_PPV_ARGS (&m_PSOs["opaque"])));

//
// PSO for opaque wireframe objects.
//
D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
ThrowIfFailed (m_Device->CreateGraphicsPipelineState (&opaqueWireframePsoDesc, IID_PPV_ARGS (&m_PSOs["opaque_wireframe"])));
}

void LandAndWavesApp::OnKeyboardInput (const GameTimer& gt) {
	if (GetAsyncKeyState ('1') & 0x8000)
		m_IsWireFrame = true;
	else
		m_IsWireFrame = false;
}

void LandAndWavesApp::UpdateCamera (const GameTimer& gt) {
	m_Eye.x = m_Radius * sinf (m_Phi) * cosf (m_Theta);
	m_Eye.y = m_Radius * cosf (m_Phi);
	m_Eye.z = m_Radius * sinf (m_Phi) * sinf (m_Theta);

	XMVECTOR pos = XMVectorSet (m_Eye.x, m_Eye.y, m_Eye.z, 1.0f);
	XMVECTOR target = XMVectorZero ();
	XMVECTOR up = XMVectorSet (0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH (pos, target, up);
	XMStoreFloat4x4 (&m_View, view);
}

void LandAndWavesApp::Update (const GameTimer& gt) {
	OnKeyboardInput (gt);
	UpdateCamera (gt);

	m_CurrFrameResourceIndex = (m_CurrFrameResourceIndex + 1) % g_NumFrameResources;
	m_CurrFrameResource = m_FrameResources[m_CurrFrameResourceIndex].get ();

	if (m_CurrFrameResource->Fence != 0 && m_Fence->GetCompletedValue () < m_CurrFrameResource->Fence) {
		HANDLE eventHandle = CreateEventEx (nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed (m_Fence->SetEventOnCompletion (m_CurrFrameResource->Fence, eventHandle));
		WaitForSingleObject (eventHandle, INFINITE);
		CloseHandle (eventHandle);
	}

	UpdateObjectCBs (gt);
	UpdateMainPassCB (gt);
	UpdateWaves (gt);
}

void LandAndWavesApp::UpdateObjectCBs (const GameTimer& gt) {
	auto currObjectCB = m_CurrFrameResource->ObjectCB.get ();
	for (auto& e : m_AllRitems) {
		if (e->NumFrameDirty > 0) {
			XMMATRIX world = XMLoadFloat4x4 (&e->World);

			ObjectConstants objConstants;
			XMStoreFloat4x4 (&objConstants.World, XMMatrixTranspose (world));

			currObjectCB->CopyData (e->ObjCBIndex, objConstants);

			e->NumFrameDirty--;
		}
	}
}

void LandAndWavesApp::UpdateMainPassCB (const GameTimer& gt) {
	XMMATRIX view = XMLoadFloat4x4 (&m_View);
	XMMATRIX proj = XMLoadFloat4x4 (&m_Proj);

	XMMATRIX viewProj = XMMatrixMultiply (view, proj);
	XMMATRIX invView = XMMatrixInverse (&XMMatrixDeterminant (view), view);
	XMMATRIX invProj = XMMatrixInverse (&XMMatrixDeterminant (proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse (&XMMatrixDeterminant (viewProj), viewProj);

	XMStoreFloat4x4 (&m_MainPassCB.View, XMMatrixTranspose (view));
	XMStoreFloat4x4 (&m_MainPassCB.InvView, XMMatrixTranspose (invView));
	XMStoreFloat4x4 (&m_MainPassCB.Proj, XMMatrixTranspose (proj));
	XMStoreFloat4x4 (&m_MainPassCB.InvProj, XMMatrixTranspose (invProj));
	XMStoreFloat4x4 (&m_MainPassCB.ViewProj, XMMatrixTranspose (viewProj));
	XMStoreFloat4x4 (&m_MainPassCB.InvViewProj, XMMatrixTranspose (invViewProj));
	m_MainPassCB.EyePosW = m_Eye;
	m_MainPassCB.RenderTargetSize = XMFLOAT2 ((float)m_ClientWidth, (float)m_ClientHeight);
	m_MainPassCB.InvRenderTargetSize = XMFLOAT2 (1.0f / m_ClientWidth, 1.0f / m_ClientHeight);
	m_MainPassCB.NearZ = 1.0f;
	m_MainPassCB.FarZ = 1000.0f;
	m_MainPassCB.TotalTime = gt.TotalTime ();
	m_MainPassCB.DeltaTime = gt.DeltaTime ();

	auto currPassCB = m_CurrFrameResource->PassCB.get ();
	currPassCB->CopyData (0, m_MainPassCB);
}

void LandAndWavesApp::UpdateWaves (const GameTimer& gt) {
	// 每隔1/4秒就要生成一個隨機波浪
	static float t_base = 0.0f;
	if (m_Timer.TotalTime () - t_base >= 0.25f) {
		t_base += 0.25f;

		int i = MathHelper::Rand (4, m_Waves->RowCount () - 5);
		int j = MathHelper::Rand (4, m_Waves->ColumnCount () - 5);

		float r = MathHelper::RandF (0.2f, 0.5f);

		m_Waves->Disturb (i, j, r);
	}

	// 更新模擬的波浪
	m_Waves->Update (gt.DeltaTime ());

	auto currWavesVB = m_CurrFrameResource->WavesVB.get ();
	for (int i = 0; i < m_Waves->VertexCount (); ++i) {
		Vertex v;

		v.Pos = m_Waves->Position (i);
		v.Color = XMFLOAT4 (DirectX::Colors::Blue);

		currWavesVB->CopyData (i, v);
	}

	// 將波浪渲染項的動態頂點緩沖區設置到當前幀的頂點緩沖區
	m_WavesRitem->Geo->VertexBufferGPU = currWavesVB->Resource ();
}

void LandAndWavesApp::Draw (const GameTimer& gt) {
	auto cmdListAlloc = m_CurrFrameResource->CmdListAlloc;

	ThrowIfFailed (cmdListAlloc->Reset ());

	if (m_IsWireFrame) {
		m_CmdList->Reset (cmdListAlloc.Get (), m_PSOs["opaque_wireframe"].Get ());
	} else {
		m_CmdList->Reset (cmdListAlloc.Get (), m_PSOs["opaque"].Get ());
	}

	m_CmdList->RSSetViewports (1, &m_Viewport);
	m_CmdList->RSSetScissorRects (1, &m_ScissorRect);

	m_CmdList->ResourceBarrier (1, &CD3DX12_RESOURCE_BARRIER::Transition (CurrentBackBuffer (),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	m_CmdList->ClearRenderTargetView (CurrentBackBufferView (), Colors::LightSteelBlue, 0, nullptr);
	m_CmdList->ClearDepthStencilView (DepthStencilView (), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	m_CmdList->OMSetRenderTargets (1, &CurrentBackBufferView (), true, &DepthStencilView ());

	m_CmdList->SetGraphicsRootSignature (m_RootSignature.Get ());

	// 綁定渲染過程中所用的常量緩沖區。在每個渲染過程中,這段代碼只需執行一次
	// 把對應的view傳進去便可
	auto passCB = m_CurrFrameResource->PassCB->Resource ();
	m_CmdList->SetGraphicsRootConstantBufferView (1, passCB->GetGPUVirtualAddress ());

	DrawRenderItems (m_CmdList.Get (), m_RitemLayer[(int)RenderLayer::Opaque]);

	m_CmdList->ResourceBarrier (1, &CD3DX12_RESOURCE_BARRIER::Transition (CurrentBackBuffer (),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed (m_CmdList->Close ());

	ID3D12CommandList* cmdsLists[] = {m_CmdList.Get ()};
	m_CmdQueue->ExecuteCommandLists (_countof (cmdsLists), cmdsLists);

	ThrowIfFailed (m_SwapChain->Present (0, 0));
	m_CurrentBackBuffer = (m_CurrentBackBuffer + 1) % m_SwapChainBufferCount;

	m_CurrFrameResource->Fence = ++m_CurrentFence;

	m_CmdQueue->Signal (m_Fence.Get (), m_CurrentFence);
}

void LandAndWavesApp::DrawRenderItems (ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems) {
	UINT objCBByteSize = D3DUtil::CalcConstantBufferByteSize (sizeof (ObjectConstants));

	auto objectCB = m_CurrFrameResource->ObjectCB->Resource ();

	for (size_t i = 0; i < ritems.size (); i++) {
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers (0, 1, &ri->Geo->VertexBufferView ());
		cmdList->IASetIndexBuffer (&ri->Geo->IndexBufferView ());
		cmdList->IASetPrimitiveTopology (ri->PrimitiveType);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress ();
		objCBAddress += ri->ObjCBIndex * objCBByteSize;

		cmdList->SetGraphicsRootConstantBufferView (0, objCBAddress);

		cmdList->DrawIndexedInstanced (ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

float LandAndWavesApp::GetHillsHeight (float x, float z) const {
	return 0.3f * (z * sinf (0.1f * x) + x * cosf (0.1f * z));
}

XMFLOAT3 LandAndWavesApp::GetHillsNormal (float x, float z) const {
	// n = (-df/dx, 1, -df/dz)
	XMFLOAT3 n (
		-0.03f * z * cosf (0.1f * x) - 0.3f * cosf (0.1f * z),
		1.0f,
		-0.3f * sinf (0.1f * x) + 0.03f * x * sinf (0.1f * z));

	XMVECTOR unitNormal = XMVector3Normalize (XMLoadFloat3 (&n));
	XMStoreFloat3 (&n, unitNormal);

	return n;
}

void LandAndWavesApp::OnMouseDown (WPARAM btnState, int x, int y) {
	m_LastMousePos.x = x;
	m_LastMousePos.y = y;

	SetCapture (m_MainWnd);
}

void LandAndWavesApp::OnMouseMove (WPARAM btnState, int x, int y) {
	if ((btnState & MK_LBUTTON) != 0) {
		float dx = XMConvertToRadians (0.25f * static_cast<float> (x - m_LastMousePos.x));
		float dy = XMConvertToRadians (0.25f * static_cast<float> (y - m_LastMousePos.y));

		m_Theta += dx;
		m_Phi += dy;

		m_Phi = MathHelper::Clamp (m_Phi, 0.1f, MathHelper::PI - 0.1f);
	} else if ((btnState & MK_RBUTTON) != 0) {
		float dx = 0.2f * static_cast<float> (x - m_LastMousePos.x);
		float dy = 0.2f * static_cast<float> (y - m_LastMousePos.y);

		m_Radius += dx - dy;

		m_Radius = MathHelper::Clamp (m_Radius, 5.0f, 150.0f);
	}

	m_LastMousePos.x = x;
	m_LastMousePos.y = y;
}

void LandAndWavesApp::OnMouseUp (WPARAM btnState, int x, int y) {
	ReleaseCapture ();
}

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int showCmd) {
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag (_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try {
		LandAndWavesApp theApp (hInstance);

		if (!theApp.Initialize ())
			return 0;

		return theApp.Run ();
	} catch (DxException& e) {
		MessageBox (nullptr, e.ToString ().c_str (), L"HR Failed", MB_OK);
		return 0;
	}
}