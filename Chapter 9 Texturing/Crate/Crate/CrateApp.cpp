#include "../../../Common/D3DApp.h"
#include "../../../Common/MathHelper.h"
#include "../../../Common/UploadBuffer.h"
#include "../../../Common/GeometryGenerator.h"
#include "../../../Common//DDSTextureLoader.h"
#include "FrameResource.h"

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

class CrateApp : public D3DApp {
public:
	CrateApp (HINSTANCE hInstance);
	CrateApp (const CrateApp& rhs) = delete;
	CrateApp& operator=(const CrateApp& rhs) = delete;
	~CrateApp ();

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

	void LoadTextures ();
	void BuildDescriptorHeaps ();
	void BuildRootSignature ();
	void BuildShadersAndInputLayout ();
	void BuildPSOs ();
	void BuildGeometries ();
	void BuildMaterials ();
	void BuildRenderItems ();
	void BuildFrameResources ();

	void DrawRenderItems (ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers ();

private:
	std::vector<std::unique_ptr<FrameResource>> m_FrameResources;
	FrameResource* m_CurrFrameResource = nullptr;
	int m_CurrFrameResourceIndex = 0;

	ComPtr<ID3D12RootSignature> m_RootSignature = nullptr;

	ComPtr<ID3D12DescriptorHeap> m_SrvDescriptorHeap = nullptr;

	std::vector<D3D12_INPUT_ELEMENT_DESC> m_InputLayout;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> m_Geometries;
	std::unordered_map<std::string, std::unique_ptr<Texture>> m_Textures;
	std::unordered_map<std::string, std::unique_ptr<Material>> m_Materials;

	std::unordered_map<std::string, ComPtr<ID3DBlob>> m_Shaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> m_PSOs;

	std::vector<std::unique_ptr<RenderItem>> m_AllRitems;
	std::vector<RenderItem*> m_RitemLayer[(int)RenderLayer::Count];

	bool m_IsWireFrame = false;

	PassConstants m_MainPassCB;

	XMFLOAT3 m_Eye = {0.0f, 0.0f, 0.0f};
	XMFLOAT4X4 m_View = MathHelper::Identity4x4 ();
	XMFLOAT4X4 m_Proj = MathHelper::Identity4x4 ();

	float m_Theta = 1.5f * XM_PI;
	float m_Phi = 0.2f * XM_PI;
	float m_Radius = 10.0f;

	POINT m_LastMousePos;
};

CrateApp::CrateApp (HINSTANCE hInstance) : D3DApp (hInstance) {
	m_MainWndCaption = L"Chapter 8 - CrateApp";
}

CrateApp::~CrateApp () {
	if (m_Device != nullptr)
		FlushCommandQueue ();
}

void CrateApp::OnResize () {
	D3DApp::OnResize ();

	XMMATRIX P = XMMatrixPerspectiveFovLH (0.25f * MathHelper::PI, AspectRatio (), 1.0f, 1000.0f);
	XMStoreFloat4x4 (&m_Proj, P);
}

bool CrateApp::Initialize () {
	if (!D3DApp::Initialize ())
		return false;

	ThrowIfFailed (m_CmdList->Reset (m_CmdAllocator.Get (), nullptr));

	LoadTextures ();
	BuildDescriptorHeaps ();
	BuildRootSignature ();
	BuildShadersAndInputLayout ();
	BuildGeometries ();
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

void CrateApp::LoadTextures () {
	auto woodCrateTex = std::make_unique<Texture> ();
	woodCrateTex->Name = "woodCrateTex";
	woodCrateTex->Filename = L"../../../Textures/WoodCrate01.dds";
	ThrowIfFailed (DirectX::CreateDDSTextureFromFile12 (m_Device.Get (),
														m_CmdList.Get (), woodCrateTex->Filename.c_str (),
														woodCrateTex->Resource, woodCrateTex->UploadHeap)
	);

	m_Textures[woodCrateTex->Name] = std::move (woodCrateTex);
}

void CrateApp::BuildDescriptorHeaps () {
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 1;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed (m_Device->CreateDescriptorHeap (&srvHeapDesc, IID_PPV_ARGS (&m_SrvDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor (m_SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart ());

	auto woodCrateTex = m_Textures["woodCrateTex"]->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = woodCrateTex->GetDesc ().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = woodCrateTex->GetDesc ().MipLevels;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	m_Device->CreateShaderResourceView (woodCrateTex.Get (), &srvDesc, hDescriptor);
}

void CrateApp::BuildRootSignature () {
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init (D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	slotRootParameter[0].InitAsDescriptorTable (1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[1].InitAsConstantBufferView (0);
	slotRootParameter[2].InitAsConstantBufferView (1);
	slotRootParameter[3].InitAsConstantBufferView (2);

	auto staticSamplers = GetStaticSamplers ();

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc (4, slotRootParameter,
											 (UINT)staticSamplers.size (), staticSamplers.data (),
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
		IID_PPV_ARGS (m_RootSignature.GetAddressOf ()))
	);
}

void CrateApp::BuildShadersAndInputLayout () {
	m_Shaders["standardVS"] = D3DUtil::CompileShader (L"Shaders/Default.hlsl", nullptr, "VS", "vs_5_0");
	m_Shaders["opaquePS"] = D3DUtil::CompileShader (L"Shaders/Default.hlsl", nullptr, "PS", "ps_5_0");

	m_InputLayout = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};
}

void CrateApp::BuildPSOs () {
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

void CrateApp::BuildGeometries () {
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox (1.0f, 1.0f, 1.0f, 3);

	UINT boxVertexOffset = 0;

	UINT boxIndexOffset = 0;

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = box.Indices32.size ();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	UINT totalVertexCount = box.Vertices.size ();

	std::vector<Vertex> vertices (totalVertexCount);
	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size (); i++, k++) {
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].TexC = box.Vertices[i].TexC;
	}

	std::vector<std::uint16_t> indices;
	indices.insert (indices.end (), std::begin (box.GetIndices16 ()), std::end (box.GetIndices16 ()));

	UINT vbSize = totalVertexCount * sizeof (Vertex);
	UINT ibSize = indices.size () * sizeof (uint16_t);

	auto geo = std::make_unique<MeshGeometry> ();
	geo->Name = "shape";

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

	geo->DrawArgs["box"] = boxSubmesh;

	m_Geometries[geo->Name] = std::move (geo);
}

void CrateApp::BuildMaterials () {
	auto woodCrate = std::make_unique<Material> ();
	woodCrate->Name = "woodCrate";
	woodCrate->MatCBIndex = 0;
	woodCrate->DiffuseSrvHeapIndex = 0;
	woodCrate->DiffuseAlbedo = XMFLOAT4 (1.0f, 1.0f, 1.0f, 1.0f);
	woodCrate->FresnelR0 = XMFLOAT3 (0.05f, 0.05f, 0.05f);
	woodCrate->Roughness = 0.99f;

	m_Materials["woodCrate"] = std::move (woodCrate);
}

void CrateApp::BuildRenderItems () {
	auto boxRitem = std::make_unique<RenderItem> ();
	boxRitem->World = MathHelper::Identity4x4 ();
	XMStoreFloat4x4 (&boxRitem->TexTransform, XMMatrixScaling (1.0f, 1.0f, 1.0f));
	boxRitem->ObjCBIndex = 0;
	boxRitem->Geo = m_Geometries["shape"].get ();
	boxRitem->Mat = m_Materials["woodCrate"].get ();
	boxRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
	m_AllRitems.push_back (std::move (boxRitem));

	for (auto& e : m_AllRitems)
		m_RitemLayer[(int)RenderLayer::Opaque].push_back (e.get ());
}

void CrateApp::BuildFrameResources () {
	for (int i = 0; i < g_NumFrameResources; i++)
		m_FrameResources.push_back (std::make_unique<FrameResource> (m_Device.Get (),
																	 1, 
																	 (UINT)m_AllRitems.size (),
																	 (UINT)m_Materials.size ()));
}

void CrateApp::OnKeyboardInput (const GameTimer& gt) {
	if (GetAsyncKeyState ('1') & 0x8000)
		m_IsWireFrame = true;
	else
		m_IsWireFrame = false;
}

void CrateApp::UpdateCamera (const GameTimer& gt) {
	m_Eye.x = m_Radius * sinf (m_Phi) * cosf (m_Theta);
	m_Eye.y = m_Radius * cosf (m_Phi);
	m_Eye.z = m_Radius * sinf (m_Phi) * sinf (m_Theta);

	XMVECTOR pos = XMVectorSet (m_Eye.x, m_Eye.y, m_Eye.z, 1.0f);
	XMVECTOR target = XMVectorZero ();
	XMVECTOR up = XMVectorSet (0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH (pos, target, up);
	XMStoreFloat4x4 (&m_View, view);
}

void CrateApp::Update (const GameTimer& gt) {
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
}

void CrateApp::UpdateObjectCBs (const GameTimer& gt) {
	auto currObjectCB = m_CurrFrameResource->ObjectCB.get ();
	for (auto& e : m_AllRitems) {
		if (e->NumFrameDirty > 0) {
			XMMATRIX world = XMLoadFloat4x4 (&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4 (&e->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4 (&objConstants.World, XMMatrixTranspose (world));
			XMStoreFloat4x4 (&objConstants.TexTransform, XMMatrixTranspose (texTransform));

			currObjectCB->CopyData (e->ObjCBIndex, objConstants);

			e->NumFrameDirty--;
		}
	}
}

void CrateApp::UpdateMainPassCB (const GameTimer& gt) {
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
	m_MainPassCB.Lights[0].Direction = {0.57735f, -0.57735f, 0.57735f};
	m_MainPassCB.Lights[0].Strength = {0.6f, 0.6f, 0.6f};
	m_MainPassCB.Lights[1].Direction = {-0.57735f, -0.57735f, 0.57735f};
	m_MainPassCB.Lights[1].Strength = {0.3f, 0.3f, 0.3f};
	m_MainPassCB.Lights[2].Direction = {0.0f, -0.707f, -0.707f};
	m_MainPassCB.Lights[2].Strength = {0.15f, 0.15f, 0.15f};

	auto currPassCB = m_CurrFrameResource->PassCB.get ();
	currPassCB->CopyData (0, m_MainPassCB);
}

void CrateApp::UpdateMaterialCBs (const GameTimer& gt) {
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

void CrateApp::Draw (const GameTimer& gt) {
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

	ID3D12DescriptorHeap* descriptorHeaps[] = {m_SrvDescriptorHeap.Get ()};
	m_CmdList->SetDescriptorHeaps (_countof (descriptorHeaps), descriptorHeaps);

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

void CrateApp::DrawRenderItems (ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems) {
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

		CD3DX12_GPU_DESCRIPTOR_HANDLE tex (m_SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart ());
		tex.Offset (ri->Mat->DiffuseSrvHeapIndex, m_CbvSrvUavDescriptorSize);

		cmdList->SetGraphicsRootDescriptorTable (0, tex);
		cmdList->SetGraphicsRootConstantBufferView (1, objCBAddress);
		cmdList->SetGraphicsRootConstantBufferView (3, matCBAddress);

		cmdList->DrawIndexedInstanced (ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

void CrateApp::OnMouseDown (WPARAM btnState, int x, int y) {
	m_LastMousePos.x = x;
	m_LastMousePos.y = y;

	SetCapture (m_MainWnd);
}

void CrateApp::OnMouseMove (WPARAM btnState, int x, int y) {
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

void CrateApp::OnMouseUp (WPARAM btnState, int x, int y) {
	ReleaseCapture ();
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> CrateApp::GetStaticSamplers () {
	// 應用程序一般只會用到這些采樣器中的一部分
	// 所以就將它們全部提前定義好,並作為根簽名的一部分保留下來

	CD3DX12_STATIC_SAMPLER_DESC pointWrap (
		0, 										// 著色器寄存器
		D3D12_FILTER_MIN_MAG_MIP_POINT, 		// 過濾器類型
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  		// U軸方向上所用的尋址模式
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  		// V軸方向上所用的尋址模式
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); 		// W軸方向上所用的尋址模式

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp (
		1, 										// 著色器寄存器
		D3D12_FILTER_MIN_MAG_MIP_POINT, 		// 過濾器類型
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  		// U軸方向上所用的尋址模式
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  		// V軸方向上所用的尋址模式
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); 		// W軸方向上所用的尋址模式

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap (
		2, 										// 著色器寄存器
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, 		// 過濾器類型
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  		// U軸方向上所用的尋址模式
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  		// V軸方向上所用的尋址模式
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); 		// W軸方向上所用的尋址模式

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp (
		3, 										// 著色器寄存器
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, 		// 過濾器類型
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  		// U軸方向上所用的尋址模式
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  		// V軸方向上所用的尋址模式
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); 		// W軸方向上所用的尋址模式

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap (
		4, 										// 著色器寄存器
		D3D12_FILTER_ANISOTROPIC, 				// 過濾器類型
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  		// U軸方向上所用的尋址模式
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  		// V軸方向上所用的尋址模式
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  		// W軸方向上所用的尋址模式
		0.0f,                             		// mipmap層級的偏置值
		8);                               		// 最大各向異性值

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp (
		5, 										// 著色器寄存器
		D3D12_FILTER_ANISOTROPIC, 				// 過濾器類型
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  		// U軸方向上所用的尋址模式
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  		// V軸方向上所用的尋址模式
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  		// W軸方向上所用的尋址模式
		0.0f,                              		// mipmap層級的偏置值
		8);                                		// 最大各向異性值

	return {pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp};
}

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE prevInstance,
					PSTR cmdLine, int showCmd) {
					// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag (_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try {
		CrateApp theApp (hInstance);

		if (!theApp.Initialize ())
			return 0;

		return theApp.Run ();
	} catch (DxException& e) {
		MessageBox (nullptr, e.ToString ().c_str (), L"HR Failed", MB_OK);
		return 0;
	}
}