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

	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4 ();

	int NumFrameDirty = g_NumFrameResources;

	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
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

class LitWavesApp : public D3DApp {
public:
	LitWavesApp (HINSTANCE hInstance);
	LitWavesApp (const LitWavesApp& rhs) = delete;
	LitWavesApp& operator=(const LitWavesApp& rhs) = delete;
	~LitWavesApp ();

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
	void UpdateMaterialCBs (const GameTimer& gt);
	void UpdateWaves (const GameTimer& gt);

	void BuildRootSignature ();
	void BuildShadersAndInputLayout ();
	void BuildPSOs ();
	void BuildLandGeometry ();
	void BuildWavesGeometryBuffers ();
	void BuildMaterials ();
	void BuildRenderItems ();
	void BuildFrameResources ();

	void DrawRenderItems (ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	float GetHillsHeight (float x, float z)const;
	XMFLOAT3 GetHillsNormal (float x, float z)const;

private:
	std::vector<std::unique_ptr<FrameResource>> m_FrameResources;
	FrameResource* m_CurrFrameResource = nullptr;
	int m_CurrFrameResourceIndex = 0;

	ComPtr<ID3D12RootSignature> m_RootSignature = nullptr;

	std::vector<D3D12_INPUT_ELEMENT_DESC> m_InputLayout;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> m_Geometries;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> m_Shaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> m_PSOs;
	std::unordered_map<std::string, std::unique_ptr<Material>> m_Materials;

	std::vector<std::unique_ptr<RenderItem>> m_AllRitems;
	std::vector<RenderItem*> m_RitemLayer[(int)RenderLayer::Count];

	std::unique_ptr<Waves> m_Waves;
	RenderItem* m_WavesRitem = nullptr;

	bool m_IsWireFrame = false;

	PassConstants m_MainPassCB;

	XMFLOAT3 m_Eye = {0.0f, 0.0f, 0.0f};
	XMFLOAT4X4 m_View = MathHelper::Identity4x4 ();
	XMFLOAT4X4 m_Proj = MathHelper::Identity4x4 ();

	float m_Theta = 1.5f * XM_PI;
	float m_Phi = XM_PIDIV2 - 0.1f;
	float m_Radius = 50.0f;

	float m_SunTheta = 1.25f * XM_PI;
	float m_SunPhi = XM_PIDIV4;

	POINT m_LastMousePos;
};

LitWavesApp::LitWavesApp (HINSTANCE hInstance) : D3DApp (hInstance) {
	m_MainWndCaption = L"Chapter 8 - LitWaves";
}

LitWavesApp::~LitWavesApp () {
	if (m_Device != nullptr)
		FlushCommandQueue ();
}

void LitWavesApp::OnResize () {
	D3DApp::OnResize ();

	XMMATRIX P = XMMatrixPerspectiveFovLH (0.25f * MathHelper::PI, AspectRatio (), 1.0f, 1000.0f);
	XMStoreFloat4x4 (&m_Proj, P);
}

bool LitWavesApp::Initialize () {
	if (!D3DApp::Initialize ())
		return false;

	ThrowIfFailed (m_CmdList->Reset (m_CmdAllocator.Get (), nullptr));

	m_Waves = std::make_unique<Waves> (128, 128, 1.0f, 0.03f, 4.0f, 0.2f);

	BuildRootSignature ();
	BuildShadersAndInputLayout ();
	BuildLandGeometry ();
	BuildWavesGeometryBuffers ();
	BuildMaterials ();
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

void LitWavesApp::BuildRootSignature () {
	CD3DX12_ROOT_PARAMETER slotRootParameter[3];
	slotRootParameter[0].InitAsConstantBufferView (0);
	slotRootParameter[1].InitAsConstantBufferView (1);
	slotRootParameter[2].InitAsConstantBufferView (2);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc (3, slotRootParameter, 0, nullptr,
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

void LitWavesApp::BuildShadersAndInputLayout () {
	m_Shaders["standardVS"] = D3DUtil::CompileShader (L"Shaders/Default.hlsl", nullptr, "VS", "vs_5_0");
	m_Shaders["opaquePS"] = D3DUtil::CompileShader (L"Shaders/Default.hlsl", nullptr, "PS", "ps_5_0");

	m_InputLayout = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
}

void LitWavesApp::BuildPSOs () {
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

void LitWavesApp::BuildLandGeometry () {
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData grid = geoGen.CreateGrid (160.0f, 160.0f, 50, 50);

	UINT gridVertexOffset = 0;

	UINT gridIndexOffset = 0;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = grid.Indices32.size ();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;

	UINT totalVertexCount = grid.Vertices.size ();

	std::vector<Vertex> vertices (totalVertexCount);
	UINT k = 0;
	for (size_t i = 0; i < grid.Vertices.size (); i++, k++) {
		auto& p = grid.Vertices[i].Position;
		vertices[k].Pos = p;
		vertices[k].Pos.y = GetHillsHeight (p.x, p.z);
		vertices[k].Normal = GetHillsNormal (p.x, p.z);
	}

	std::vector<std::uint16_t> indices;
	indices.insert (indices.end (), std::begin (grid.GetIndices16 ()), std::end (grid.GetIndices16 ()));

	UINT vbSize = totalVertexCount * sizeof (Vertex);
	UINT ibSize = indices.size () * sizeof (uint16_t);

	auto geo = std::make_unique<MeshGeometry> ();
	geo->Name = "land";

	ThrowIfFailed (D3DCreateBlob (vbSize, &geo->VertexBufferCPU));
	CopyMemory (geo->VertexBufferCPU->GetBufferPointer (), vertices.data (), vbSize);

	ThrowIfFailed (D3DCreateBlob (ibSize, &geo->IndexBufferCPU));
	CopyMemory (geo->IndexBufferCPU->GetBufferPointer (), indices.data (), ibSize);

	geo->VertexBufferGPU = D3DUtil::CreateDefaultBuffer (m_Device.Get (),
														 m_CmdList.Get (), vertices.data (), vbSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = D3DUtil::CreateDefaultBuffer (m_Device.Get (),
														m_CmdList.Get (), indices.data (), ibSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof (Vertex);
	geo->VertexBufferByteSize = vbSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibSize;

	geo->DrawArgs["land"] = gridSubmesh;

	m_Geometries[geo->Name] = std::move (geo);
}

void LitWavesApp::BuildWavesGeometryBuffers () {
	std::vector<std::uint16_t> indices (3 * m_Waves->TriangleCount ()); // 3 indices per face
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

			k += 6; // next quad
		}
	}

	UINT vbByteSize = m_Waves->VertexCount () * sizeof (Vertex);
	UINT ibByteSize = (UINT)indices.size () * sizeof (std::uint16_t);

	auto geo = std::make_unique<MeshGeometry> ();
	geo->Name = "water";

	// Set dynamically.
	geo->VertexBufferCPU = nullptr;
	geo->VertexBufferGPU = nullptr;

	ThrowIfFailed (D3DCreateBlob (ibByteSize, &geo->IndexBufferCPU));
	CopyMemory (geo->IndexBufferCPU->GetBufferPointer (), indices.data (), ibByteSize);

	geo->IndexBufferGPU = D3DUtil::CreateDefaultBuffer (m_Device.Get (),
														m_CmdList.Get (), indices.data (), ibByteSize, geo->IndexBufferUploader);

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

void LitWavesApp::BuildMaterials () {
	auto grass = std::make_unique<Material> ();
	grass->Name = "grass";
	grass->MatCBIndex = 0;
	grass->DiffuseAlbedo = XMFLOAT4 (0.2f, 0.6f, 0.2f, 1.0f);
	grass->FresnelR0 = XMFLOAT3 (0.01f, 0.01f, 0.01f);
	grass->Roughness = 0.125f;

	auto water = std::make_unique<Material> ();
	water->Name = "water";
	water->MatCBIndex = 1;
	water->DiffuseAlbedo = XMFLOAT4 (0.0f, 0.2f, 0.6f, 1.0f);
	water->FresnelR0 = XMFLOAT3 (0.1f, 0.1f, 0.1f);
	water->Roughness = 0.0f;

	m_Materials["grass"] = std::move (grass);
	m_Materials["water"] = std::move (water);
}

void LitWavesApp::BuildRenderItems () {
	auto wavesRitem = std::make_unique<RenderItem> ();
	wavesRitem->World = MathHelper::Identity4x4 ();
	wavesRitem->ObjCBIndex = 0;
	wavesRitem->Mat = m_Materials["water"].get ();
	wavesRitem->Geo = m_Geometries["water"].get ();
	wavesRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wavesRitem->IndexCount = wavesRitem->Geo->DrawArgs["water"].IndexCount;
	wavesRitem->StartIndexLocation = wavesRitem->Geo->DrawArgs["water"].StartIndexLocation;
	wavesRitem->BaseVertexLocation = wavesRitem->Geo->DrawArgs["water"].BaseVertexLocation;

	m_WavesRitem = wavesRitem.get ();

	auto landRitem = std::make_unique<RenderItem> ();
	landRitem->World = MathHelper::Identity4x4 ();
	XMStoreFloat4x4 (&landRitem->TexTransform, XMMatrixScaling (1.0f, 1.0f, 1.0f));
	landRitem->ObjCBIndex = 1;
	landRitem->Geo = m_Geometries["land"].get ();
	landRitem->Mat = m_Materials["grass"].get ();
	landRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	landRitem->IndexCount = landRitem->Geo->DrawArgs["land"].IndexCount;
	landRitem->StartIndexLocation = landRitem->Geo->DrawArgs["land"].StartIndexLocation;
	landRitem->BaseVertexLocation = landRitem->Geo->DrawArgs["land"].BaseVertexLocation;

	m_AllRitems.push_back (std::move (landRitem));
	m_AllRitems.push_back (std::move (wavesRitem));

	for (auto& e : m_AllRitems)
		m_RitemLayer[(int)RenderLayer::Opaque].push_back (e.get ());
}

void LitWavesApp::BuildFrameResources () {
	for (int i = 0; i < g_NumFrameResources; i++)
		m_FrameResources.push_back (std::make_unique<FrameResource> (m_Device.Get (), 1, (UINT)m_AllRitems.size (), (UINT)m_Materials.size (), m_Waves->VertexCount ()));
}

void LitWavesApp::OnKeyboardInput (const GameTimer& gt) {
	if (GetAsyncKeyState ('1') & 0x8000)
		m_IsWireFrame = true;
	else
		m_IsWireFrame = false;

	const float dt = gt.DeltaTime ();

	if (GetAsyncKeyState (VK_LEFT) & 0x8000)
		m_SunTheta -= 1.0f * dt;

	if (GetAsyncKeyState (VK_RIGHT) & 0x8000)
		m_SunTheta += 1.0f * dt;

	if (GetAsyncKeyState (VK_UP) & 0x8000)
		m_SunPhi -= 1.0f * dt;

	if (GetAsyncKeyState (VK_DOWN) & 0x8000)
		m_SunPhi += 1.0f * dt;

	m_SunPhi = MathHelper::Clamp (m_SunPhi, 0.1f, XM_PIDIV2);
}

void LitWavesApp::UpdateCamera (const GameTimer& gt) {
	m_Eye.x = m_Radius * sinf (m_Phi) * cosf (m_Theta);
	m_Eye.y = m_Radius * cosf (m_Phi);
	m_Eye.z = m_Radius * sinf (m_Phi) * sinf (m_Theta);

	XMVECTOR pos = XMVectorSet (m_Eye.x, m_Eye.y, m_Eye.z, 1.0f);
	XMVECTOR target = XMVectorZero ();
	XMVECTOR up = XMVectorSet (0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH (pos, target, up);
	XMStoreFloat4x4 (&m_View, view);
}

void LitWavesApp::Update (const GameTimer& gt) {
	UpdateCamera (gt);
	OnKeyboardInput (gt);

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
	UpdateMaterialCBs (gt);
	UpdateWaves (gt);
}

void LitWavesApp::UpdateObjectCBs (const GameTimer& gt) {
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

void LitWavesApp::UpdateMainPassCB (const GameTimer& gt) {
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

	m_MainPassCB.AmbientLight = {0.25f, 0.25f, 0.35f, 1.0f};

	XMVECTOR lightDir = -MathHelper::SphericalToCartesian (1.0f, m_SunTheta, m_SunPhi);

	XMStoreFloat3 (&m_MainPassCB.Lights[0].Direction, lightDir);
	m_MainPassCB.Lights[0].Strength = {1.0f, 1.0f, 0.9f};

	auto currPassCB = m_CurrFrameResource->PassCB.get ();
	currPassCB->CopyData (0, m_MainPassCB);
}

void LitWavesApp::UpdateMaterialCBs (const GameTimer& gt) {
	auto currMaterialCB = m_CurrFrameResource->MaterialCB.get ();
	for (auto& e : m_Materials) {
		Material* mat = e.second.get ();
		if (mat->NumFrameDirty > 0) {
			XMMATRIX matTransform = XMLoadFloat4x4 (&mat->MatTransform);

			MaterialConstants matConstnats;
			matConstnats.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstnats.FresnelR0 = mat->FresnelR0;
			matConstnats.Roughness = mat->Roughness;
			XMStoreFloat4x4 (&matConstnats.MatTransform, XMMatrixTranspose (matTransform));

			currMaterialCB->CopyData (mat->MatCBIndex, matConstnats);

			mat->NumFrameDirty--;
		}
	}
}

void LitWavesApp::UpdateWaves (const GameTimer& gt) {
	// Every quarter second, generate a random wave.
	static float t_base = 0.0f;
	if ((m_Timer.TotalTime () - t_base) >= 0.25f) {
		t_base += 0.25f;

		int i = MathHelper::Rand (4, m_Waves->RowCount () - 5);
		int j = MathHelper::Rand (4, m_Waves->ColumnCount () - 5);

		float r = MathHelper::RandF (0.2f, 0.5f);

		m_Waves->Disturb (i, j, r);
	}

	// Update the wave simulation.
	m_Waves->Update (gt.DeltaTime ());

	// Update the wave vertex buffer with the new solution.
	auto currWavesVB = m_CurrFrameResource->WavesVB.get ();
	for (int i = 0; i < m_Waves->VertexCount (); ++i) {
		Vertex v;

		v.Pos = m_Waves->Position (i);
		v.Normal = m_Waves->Normal (i);

		currWavesVB->CopyData (i, v);
	}

	// Set the dynamic VB of the wave renderitem to the current frame VB.
	m_WavesRitem->Geo->VertexBufferGPU = currWavesVB->Resource ();
}

void LitWavesApp::Draw (const GameTimer& gt) {
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

	auto passCB = m_CurrFrameResource->PassCB->Resource ();
	m_CmdList->SetGraphicsRootConstantBufferView (2, passCB->GetGPUVirtualAddress ());

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

void LitWavesApp::DrawRenderItems (ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems) {
	UINT objCBByteSize = D3DUtil::CalcConstantBufferByteSize (sizeof (ObjectConstants));
	UINT matCBByteSize = D3DUtil::CalcConstantBufferByteSize (sizeof (MaterialConstants));

	auto objectCB = m_CurrFrameResource->ObjectCB->Resource ();
	auto matCB = m_CurrFrameResource->MaterialCB->Resource ();

	for (size_t i = 0; i < ritems.size (); i++) {
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers (0, 1, &ri->Geo->VertexBufferView ());
		cmdList->IASetIndexBuffer (&ri->Geo->IndexBufferView ());
		cmdList->IASetPrimitiveTopology (ri->PrimitiveType);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress ();
		objCBAddress += ri->ObjCBIndex * objCBByteSize;

		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress ();
		matCBAddress += ri->Mat->MatCBIndex * matCBByteSize;

		cmdList->SetGraphicsRootConstantBufferView (0, objCBAddress);
		cmdList->SetGraphicsRootConstantBufferView (1, matCBAddress);

		cmdList->DrawIndexedInstanced (ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

void LitWavesApp::OnMouseDown (WPARAM btnState, int x, int y) {
	m_LastMousePos.x = x;
	m_LastMousePos.y = y;

	SetCapture (m_MainWnd);
}

void LitWavesApp::OnMouseMove (WPARAM btnState, int x, int y) {
	if ((btnState & MK_LBUTTON) != 0) {
		float dx = XMConvertToRadians (0.25f * static_cast<float> (m_LastMousePos.x - x));
		float dy = XMConvertToRadians (0.25f * static_cast<float> (m_LastMousePos.y - y));

		m_Theta += dx;
		m_Phi += dy;

		m_Phi = MathHelper::Clamp (m_Phi, 0.1f, MathHelper::PI - 0.1f);
	} else if ((btnState & MK_RBUTTON) != 0) {
		float dx = 0.05f * static_cast<float> (x - m_LastMousePos.x);
		float dy = 0.05f * static_cast<float> (y - m_LastMousePos.y);

		m_Radius += dx - dy;

		m_Radius = MathHelper::Clamp (m_Radius, 5.0f, 150.0f);
	}

	m_LastMousePos.x = x;
	m_LastMousePos.y = y;
}

void LitWavesApp::OnMouseUp (WPARAM btnState, int x, int y) {
	ReleaseCapture ();
}

float LitWavesApp::GetHillsHeight (float x, float z)const {
	return 0.3f * (z * sinf (0.1f * x) + x * cosf (0.1f * z));
}

XMFLOAT3 LitWavesApp::GetHillsNormal (float x, float z)const {
	// n = (-df/dx, 1, -df/dz)
	XMFLOAT3 n (
		-0.03f * z * cosf (0.1f * x) - 0.3f * cosf (0.1f * z),
		1.0f,
		-0.3f * sinf (0.1f * x) + 0.03f * x * sinf (0.1f * z)
	);

	XMVECTOR unitNormal = XMVector3Normalize (XMLoadFloat3 (&n));
	XMStoreFloat3 (&n, unitNormal);

	return n;
}

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE prevInstance,
					PSTR cmdLine, int showCmd) {
					// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag (_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try {
		LitWavesApp theApp (hInstance);

		if (!theApp.Initialize ())
			return 0;

		return theApp.Run ();
	} catch (DxException& e) {
		MessageBox (nullptr, e.ToString ().c_str (), L"HR Failed", MB_OK);
		return 0;
	}
}