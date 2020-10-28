#include "../../../Common/D3DApp.h"
#include "../../../Common/MathHelper.h"
#include "../../../Common/UploadBuffer.h"
#include "../../../Common/GeometryGenerator.h"
#include "../../../Common/DDSTextureLoader.h"
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

class TexWavesApp : public D3DApp {
public:
	TexWavesApp (HINSTANCE hInstance);
	TexWavesApp (const TexWavesApp& rhs) = delete;
	TexWavesApp& operator=(const TexWavesApp& rhs) = delete;
	~TexWavesApp ();

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
	void AnimateMaterials (const GameTimer& gt);

	void LoadTextures ();
	void BuildRootSignature ();
	void BuildDescriptorHeaps ();
	void BuildShadersAndInputLayout ();
	void BuildPSOs ();
	void BuildLandGeometry ();
	void BuildWavesGeometry ();
	void BuildBoxGeometry ();
	void BuildMaterials ();
	void BuildRenderItems ();
	void BuildFrameResources ();

	void DrawRenderItems (ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers ();

	float GetHillsHeight (float x, float z)const;
	XMFLOAT3 GetHillsNormal (float x, float z)const;

private:
	std::vector<std::unique_ptr<FrameResource>> m_FrameResources;
	FrameResource* m_CurrFrameResource = nullptr;
	int m_CurrFrameResourceIndex = 0;

	ComPtr<ID3D12RootSignature> m_RootSignature = nullptr;

	ComPtr<ID3D12DescriptorHeap> m_SrvDescriptorHeap = nullptr;

	std::vector<D3D12_INPUT_ELEMENT_DESC> m_InputLayout;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> m_Geometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> m_Materials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> m_Textures;

	std::unordered_map<std::string, ComPtr<ID3DBlob>> m_Shaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> m_PSOs;

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

	POINT m_LastMousePos;
};

TexWavesApp::TexWavesApp (HINSTANCE hInstance) : D3DApp (hInstance) {
	m_MainWndCaption = L"Chapter - 9 - TexWaves";
}

TexWavesApp::~TexWavesApp () {
	if (m_Device != nullptr)
		FlushCommandQueue ();
}

void TexWavesApp::OnResize () {
	D3DApp::OnResize ();

	XMMATRIX P = XMMatrixPerspectiveFovLH (0.25f * MathHelper::PI, AspectRatio (), 1.0f, 1000.0f);
	XMStoreFloat4x4 (&m_Proj, P);
}

bool TexWavesApp::Initialize () {
	if (!D3DApp::Initialize ())
		return false;

	ThrowIfFailed (m_CmdList->Reset (m_CmdAllocator.Get (), nullptr));

	m_Waves = std::make_unique<Waves> (128, 128, 1.0f, 0.03f, 4.0f, 0.2f);

	LoadTextures ();
	BuildRootSignature ();
	BuildDescriptorHeaps ();
	BuildShadersAndInputLayout ();
	BuildPSOs ();
	BuildLandGeometry ();
	BuildWavesGeometry ();
	BuildBoxGeometry ();
	BuildMaterials ();
	BuildRenderItems ();
	BuildFrameResources ();

	ThrowIfFailed (m_CmdList->Close ());
	ID3D12CommandList* cmdsLists[] = {m_CmdList.Get ()};
	m_CmdQueue->ExecuteCommandLists (_countof (cmdsLists), cmdsLists);

	FlushCommandQueue ();

	for (auto& geo : m_Geometries) {
		geo.second->DisposeUploaders ();
	}

	return true;
}

void TexWavesApp::LoadTextures () {
	auto grassTex = std::make_unique<Texture> ();
	grassTex->Name = "grassTex";
	grassTex->Filename = L"../../../Textures/grass.dds";
	ThrowIfFailed (DirectX::CreateDDSTextureFromFile12 (m_Device.Get (),
														m_CmdList.Get (), grassTex->Filename.c_str (),
														grassTex->Resource, grassTex->UploadHeap));

	auto waterTex = std::make_unique<Texture> ();
	waterTex->Name = "waterTex";
	waterTex->Filename = L"../../../Textures/water1.dds";
	ThrowIfFailed (DirectX::CreateDDSTextureFromFile12 (m_Device.Get (),
														m_CmdList.Get (), waterTex->Filename.c_str (),
														waterTex->Resource, waterTex->UploadHeap));

	auto fenceTex = std::make_unique<Texture> ();
	fenceTex->Name = "fenceTex";
	fenceTex->Filename = L"../../../Textures/WoodCrate01.dds";
	ThrowIfFailed (DirectX::CreateDDSTextureFromFile12 (m_Device.Get (),
														m_CmdList.Get (), fenceTex->Filename.c_str (),
														fenceTex->Resource, fenceTex->UploadHeap));

	m_Textures[grassTex->Name] = std::move (grassTex);
	m_Textures[waterTex->Name] = std::move (waterTex);
	m_Textures[fenceTex->Name] = std::move (fenceTex);
}

void TexWavesApp::BuildDescriptorHeaps () {
	//
	// Create SRV Heap
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 3;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed (m_Device->CreateDescriptorHeap (&srvHeapDesc, IID_PPV_ARGS (&m_SrvDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor (m_SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart ());

	auto grassTex = m_Textures["grassTex"]->Resource;
	auto waterTex = m_Textures["waterTex"]->Resource;
	auto fenceTex = m_Textures["fenceTex"]->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = grassTex->GetDesc ().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = -1;
	m_Device->CreateShaderResourceView (grassTex.Get (), &srvDesc, hDescriptor);

	hDescriptor.Offset (1, m_CbvSrvUavDescriptorSize);

	srvDesc.Format = waterTex->GetDesc ().Format;
	m_Device->CreateShaderResourceView (waterTex.Get (), &srvDesc, hDescriptor);

	hDescriptor.Offset (1, m_CbvSrvUavDescriptorSize);

	srvDesc.Format = fenceTex->GetDesc ().Format;
	m_Device->CreateShaderResourceView (fenceTex.Get (), &srvDesc, hDescriptor);
}

void TexWavesApp::BuildRootSignature () {
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
		IID_PPV_ARGS (m_RootSignature.GetAddressOf ())
	));
}

void TexWavesApp::BuildShadersAndInputLayout () {
	m_Shaders["standardVS"] = D3DUtil::CompileShader (L"Shaders/Default.hlsl", nullptr, "VS", "vs_5_0");
	m_Shaders["opaquePS"] = D3DUtil::CompileShader (L"Shaders/Default.hlsl", nullptr, "PS", "ps_5_0");

	m_InputLayout = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};
}

void TexWavesApp::BuildPSOs () {
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

void TexWavesApp::BuildLandGeometry () {
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData land = geoGen.CreateGrid (160.0f, 160.0f, 50, 50);

	UINT boxVertexOffset = 0;

	UINT boxIndexOffset = 0;

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = land.Indices32.size ();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	UINT totalVertexCount = land.Vertices.size ();

	std::vector<Vertex> vertices (totalVertexCount);
	UINT k = 0;
	for (size_t i = 0; i < land.Vertices.size (); i++, k++) {
		auto& p = land.Vertices[i].Position;
		vertices[k].Pos = p;
		vertices[k].Pos.y = GetHillsHeight (p.x, p.z);
		vertices[k].Normal = GetHillsNormal (p.x, p.z);
		vertices[k].TexC = land.Vertices[i].TexC;
	}

	std::vector<std::uint16_t> indices;
	indices.insert (indices.end (), std::begin (land.GetIndices16 ()), std::end (land.GetIndices16 ()));

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

	geo->DrawArgs["land"] = boxSubmesh;

	m_Geometries[geo->Name] = std::move (geo);
}

void TexWavesApp::BuildWavesGeometry () {
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

void TexWavesApp::BuildBoxGeometry () {
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox (8.0f, 8.0f, 8.0f, 3);

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
	geo->Name = "box";

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

void TexWavesApp::BuildMaterials () {
	auto grass = std::make_unique<Material> ();
	grass->Name = "grass";
	grass->MatCBIndex = 0;
	grass->DiffuseSrvHeapIndex = 0;
	grass->DiffuseAlbedo = XMFLOAT4 (1.0f, 1.0f, 1.0f, 1.0f);
	grass->FresnelR0 = XMFLOAT3 (0.01f, 0.01f, 0.01f);
	grass->Roughness = 0.99f;

	// This is not a good water material definition, but we do not have all the rendering
	// tools we need (transparency, environment reflection), so we fake it for now.
	auto water = std::make_unique<Material> ();
	water->Name = "water";
	water->MatCBIndex = 1;
	water->DiffuseSrvHeapIndex = 1;
	water->DiffuseAlbedo = XMFLOAT4 (1.0f, 1.0f, 1.0f, 1.0f);
	water->FresnelR0 = XMFLOAT3 (0.2f, 0.2f, 0.2f);
	water->Roughness = 0.0f;

	auto wirefence = std::make_unique<Material> ();
	wirefence->Name = "wirefence";
	wirefence->MatCBIndex = 2;
	wirefence->DiffuseSrvHeapIndex = 2;
	wirefence->DiffuseAlbedo = XMFLOAT4 (1.0f, 1.0f, 1.0f, 1.0f);
	wirefence->FresnelR0 = XMFLOAT3 (0.1f, 0.1f, 0.1f);
	wirefence->Roughness = 0.25f;

	m_Materials["grass"] = std::move (grass);
	m_Materials["water"] = std::move (water);
	m_Materials["wirefence"] = std::move (wirefence);
}

void TexWavesApp::BuildRenderItems () {
	auto wavesRitem = std::make_unique<RenderItem> ();
	wavesRitem->World = MathHelper::Identity4x4 ();
	XMStoreFloat4x4 (&wavesRitem->TexTransform, XMMatrixScaling (5.0f, 5.0f, 1.0f));
	wavesRitem->ObjCBIndex = 0;
	wavesRitem->Mat = m_Materials["water"].get ();
	wavesRitem->Geo = m_Geometries["water"].get ();
	wavesRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wavesRitem->IndexCount = wavesRitem->Geo->DrawArgs["water"].IndexCount;
	wavesRitem->StartIndexLocation = wavesRitem->Geo->DrawArgs["water"].StartIndexLocation;
	wavesRitem->BaseVertexLocation = wavesRitem->Geo->DrawArgs["water"].BaseVertexLocation;

	m_WavesRitem = wavesRitem.get ();

	m_RitemLayer[(int)RenderLayer::Opaque].push_back (wavesRitem.get ());

	auto landRitem = std::make_unique<RenderItem> ();
	landRitem->World = MathHelper::Identity4x4 ();
	XMStoreFloat4x4 (&landRitem->TexTransform, XMMatrixScaling (5.0f, 5.0f, 1.0f));
	landRitem->ObjCBIndex = 1;
	landRitem->Mat = m_Materials["grass"].get ();
	landRitem->Geo = m_Geometries["land"].get ();
	landRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	landRitem->IndexCount = landRitem->Geo->DrawArgs["land"].IndexCount;
	landRitem->StartIndexLocation = landRitem->Geo->DrawArgs["land"].StartIndexLocation;
	landRitem->BaseVertexLocation = landRitem->Geo->DrawArgs["land"].BaseVertexLocation;

	m_RitemLayer[(int)RenderLayer::Opaque].push_back (landRitem.get ());

	auto boxRitem = std::make_unique<RenderItem> ();
	XMStoreFloat4x4 (&boxRitem->World, XMMatrixTranslation (3.0f, 2.0f, -9.0f));
	boxRitem->ObjCBIndex = 2;
	boxRitem->Mat = m_Materials["wirefence"].get ();
	boxRitem->Geo = m_Geometries["box"].get ();
	boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;

	m_RitemLayer[(int)RenderLayer::Opaque].push_back (boxRitem.get ());

	m_AllRitems.push_back (std::move (wavesRitem));
	m_AllRitems.push_back (std::move (landRitem));
	m_AllRitems.push_back (std::move (boxRitem));
}

void TexWavesApp::BuildFrameResources () {
	for (int i = 0; i < g_NumFrameResources; i++)
		m_FrameResources.push_back (std::make_unique<FrameResource> (m_Device.Get (), 1, (UINT)m_AllRitems.size (), (UINT)m_Materials.size (), m_Waves->VertexCount ()));
}

void TexWavesApp::OnKeyboardInput (const GameTimer& gt) {
	if (GetAsyncKeyState ('1') & 0x8000)
		m_IsWireFrame = true;
	else
		m_IsWireFrame = false;
}

void TexWavesApp::UpdateCamera (const GameTimer& gt) {
	m_Eye.x = m_Radius * sinf (m_Phi) * cosf (m_Theta);
	m_Eye.y = m_Radius * cosf (m_Phi);
	m_Eye.z = m_Radius * sinf (m_Phi) * sinf (m_Theta);

	XMVECTOR pos = XMVectorSet (m_Eye.x, m_Eye.y, m_Eye.z, 1.0f);
	XMVECTOR target = XMVectorZero ();
	XMVECTOR up = XMVectorSet (0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH (pos, target, up);
	XMStoreFloat4x4 (&m_View, view);
}

void TexWavesApp::Update (const GameTimer& gt) {
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

	AnimateMaterials (gt);
	UpdateObjectCBs (gt);
	UpdateMainPassCB (gt);
	UpdateMaterialCBs (gt);
	UpdateWaves (gt);
}

void TexWavesApp::AnimateMaterials (const GameTimer& gt) {
	// 使水流材質的紋理坐標滾動起來
	auto waterMat = m_Materials["water"].get ();

	float& tu = waterMat->MatTransform (3, 0);	// u軸平移
	float& tv = waterMat->MatTransform (3, 1);	// v軸平移

	
	tu += MathHelper::RandF (0.05f, 0.15f) * gt.DeltaTime ();
	tv += MathHelper::RandF (0.02f, 0.05f) * gt.DeltaTime ();

	if (tu >= 1.0f)
		tu -= 1.0f;

	if (tv >= 1.0f)
		tv -= 1.0f;

	//waterMat->MatTransform (3, 0) = tu;
	//waterMat->MatTransform (3, 1) = tv;

	//材質已發生變化, 因而需要更新常量緩沖區
	waterMat->NumFrameDirty = g_NumFrameResources;
}

void TexWavesApp::UpdateObjectCBs (const GameTimer& gt) {
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

void TexWavesApp::UpdateMainPassCB (const GameTimer& gt) {
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
	m_MainPassCB.Lights[0].Strength = {0.9f, 0.9f, 0.9f};
	m_MainPassCB.Lights[1].Direction = {-0.57735f, -0.57735f, 0.57735f};
	m_MainPassCB.Lights[1].Strength = {0.5f, 0.5f, 0.5f};
	m_MainPassCB.Lights[2].Direction = {0.0f, -0.707f, -0.707f};
	m_MainPassCB.Lights[2].Strength = {0.2f, 0.2f, 0.2f};

	auto currPassCB = m_CurrFrameResource->PassCB.get ();
	currPassCB->CopyData (0, m_MainPassCB);
}

void TexWavesApp::UpdateMaterialCBs (const GameTimer& gt) {
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

void TexWavesApp::UpdateWaves (const GameTimer& gt) {
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
		// 將頂點位置(歸一化)映射到UV的[0, 1]上
		v.TexC.x = 0.5f + v.Pos.x / m_Waves->Width ();
		v.TexC.y = 0.5f - v.Pos.z / m_Waves->Depth ();

		currWavesVB->CopyData (i, v);
	}

	// Set the dynamic VB of the wave renderitem to the current frame VB.
	m_WavesRitem->Geo->VertexBufferGPU = currWavesVB->Resource ();
}

void TexWavesApp::Draw (const GameTimer& gt) {
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

void TexWavesApp::DrawRenderItems (ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems) {
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

void TexWavesApp::OnMouseDown (WPARAM btnState, int x, int y) {
	m_LastMousePos.x = x;
	m_LastMousePos.y = y;

	SetCapture (m_MainWnd);
}

void TexWavesApp::OnMouseMove (WPARAM btnState, int x, int y) {
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

void TexWavesApp::OnMouseUp (WPARAM btnState, int x, int y) {
	ReleaseCapture ();
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> TexWavesApp::GetStaticSamplers () {
	// 應用程序一般只會用到這些采樣器的一部分
	// 所以就將他們全部提前定義好,並作為根簽名的一部分保留下來

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap (
		0,										// 着色器寄存器
		D3D12_FILTER_MIN_MAG_MIP_POINT,			// 過濾器類型
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// U軸方向上所用的尋址模式
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// V軸方向上所用的尋址模式
		D3D12_TEXTURE_ADDRESS_MODE_WRAP			// W軸方向上所用的尋址模式
	);

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp (
		1,										// 着色器寄存器
		D3D12_FILTER_MIN_MAG_MIP_POINT,			// 過濾器類型
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// U軸方向上所用的尋址模式
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// V軸方向上所用的尋址模式
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP		// W軸方向上所用的尋址模式
	);

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap (
		2,										// 着色器寄存器
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,		// 過濾器類型
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// U軸方向上所用的尋址模式
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// V軸方向上所用的尋址模式
		D3D12_TEXTURE_ADDRESS_MODE_WRAP			// W軸方向上所用的尋址模式
	);

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp (
		3,										// 着色器寄存器
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,		// 過濾器類型
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// U軸方向上所用的尋址模式
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// V軸方向上所用的尋址模式
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP		// W軸方向上所用的尋址模式
	);

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap (
		4,										// 着色器寄存器
		D3D12_FILTER_ANISOTROPIC,				// 過濾器類型
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// U軸方向上所用的尋址模式
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// V軸方向上所用的尋址模式
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// W軸方向上所用的尋址模式
		0.0f,									// mipmap層級的偏置值
		8										// 最大各向異性值
	);

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp (
		5,										// 着色器寄存器
		D3D12_FILTER_ANISOTROPIC,				// 過濾器類型
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// U軸方向上所用的尋址模式
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// V軸方向上所用的尋址模式
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// W軸方向上所用的尋址模式
		0.0f,									// mipmap層級的偏置值
		8										// 最大各向異性值
	);

	return {pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp};
}

float TexWavesApp::GetHillsHeight (float x, float z)const {
	return 0.3f * (z * sinf (0.1f * x) + x * cosf (0.1f * z));
}

XMFLOAT3 TexWavesApp::GetHillsNormal (float x, float z)const {
	XMFLOAT3 n (
		-0.03f * z * cosf (0.1f * x) - 0.3f * cosf (0.1f * z),
		1.0f,
		-0.3f * sinf (0.1f * x) + 0.03f * x * sinf (0.1f * z));

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
		TexWavesApp theApp (hInstance);

		if (!theApp.Initialize ())
			return 0;

		return theApp.Run ();
	} catch (DxException& e) {
		MessageBox (nullptr, e.ToString ().c_str (), L"HR Failed", MB_OK);
		return 0;
	}
}