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
	Transparent,
	AlphaTested,
	Count
};

class BlendDemoApp : public D3DApp {
public:
	BlendDemoApp (HINSTANCE hInstance);
	BlendDemoApp (const BlendDemoApp& rhs) = delete;
	BlendDemoApp& operator=(const BlendDemoApp& rhs) = delete;
	~BlendDemoApp ();

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
	void AnimateMaterials (const GameTimer& gt);
	void UpdateWaves (const GameTimer& gt);

	void LoadTextures ();
	void BuildScene ();
	void BuildWaves ();
	void BuildMaterials ();
	void BuildRenderItems ();
	void BuildDescriptorHeaps ();
	void BuildRootSignature ();
	void BuildShadersAndInputLayout ();
	void BuildPSOs ();
	void BuildFrameResources ();

	void LoadDefultSceneTextures ();
	void BuildDefaultScene ();
	void BuildDefaultSceneMaterials ();
	void BuildDefaultSceneRenderItems ();
	void BuildDefaultSceneDescriptorHeaps ();

	void DrawRenderItems (ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	float GetHillsHeight (float x, float z) const;
	XMFLOAT3 GetHillsNormal (float x, float z) const;

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

BlendDemoApp::BlendDemoApp (HINSTANCE hInstance) : D3DApp (hInstance) {
	m_MainWndCaption = L"Chapter - ";
}

BlendDemoApp::~BlendDemoApp () {
	if (m_Device != nullptr)
		FlushCommandQueue ();
}

void BlendDemoApp::OnResize () {
	D3DApp::OnResize ();

	XMMATRIX P = XMMatrixPerspectiveFovLH (0.25f * MathHelper::PI, AspectRatio (), 1.0f, 1000.0f);
	XMStoreFloat4x4 (&m_Proj, P);
}

bool BlendDemoApp::Initialize () {
	if (!D3DApp::Initialize ())
		return false;

	ThrowIfFailed (m_CmdList->Reset (m_CmdAllocator.Get (), nullptr));
	
	//LoadDefultSceneTextures ();
	//BuildDefaultScene ();
	//BuildDefaultSceneMaterials ();
	//BuildDefaultSceneRenderItems ();
	//BuildDefaultSceneDescriptorHeaps ();

	m_Waves = std::make_unique<Waves> (128, 128, 1.0f, 0.03f, 4.0f, 0.2f);

	LoadTextures ();
	BuildScene ();
	BuildWaves ();
	BuildMaterials ();
	BuildRenderItems ();
	BuildDescriptorHeaps ();
	BuildRootSignature ();
	BuildShadersAndInputLayout ();
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

#pragma region Default Scene

void BlendDemoApp::LoadDefultSceneTextures () {
	auto bricksTex = std::make_unique<Texture> ();
	bricksTex->Name = "bricksTex";
	bricksTex->Filename = L"../../../Textures/bricks.dds";
	ThrowIfFailed (DirectX::CreateDDSTextureFromFile12 (m_Device.Get (),
														m_CmdList.Get (), bricksTex->Filename.c_str (),
														bricksTex->Resource, bricksTex->UploadHeap)
	);

	auto stoneTex = std::make_unique<Texture> ();
	stoneTex->Name = "stoneTex";
	stoneTex->Filename = L"../../../Textures/stone.dds";
	ThrowIfFailed (DirectX::CreateDDSTextureFromFile12 (m_Device.Get (),
														m_CmdList.Get (), stoneTex->Filename.c_str (),
														stoneTex->Resource, stoneTex->UploadHeap)
	);

	auto tileTex = std::make_unique<Texture> ();
	tileTex->Name = "tileTex";
	tileTex->Filename = L"../../../Textures/tile.dds";
	ThrowIfFailed (DirectX::CreateDDSTextureFromFile12 (m_Device.Get (),
														m_CmdList.Get (), tileTex->Filename.c_str (),
														tileTex->Resource, tileTex->UploadHeap)
	);

	m_Textures[bricksTex->Name] = std::move (bricksTex);
	m_Textures[stoneTex->Name] = std::move (stoneTex);
	m_Textures[tileTex->Name] = std::move (tileTex);
}

void BlendDemoApp::BuildDefaultScene () {
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox (1.0f, 1.0f, 1.0f, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid (20.0f, 30.0f, 60, 40);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere (0.5f, 20, 20);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder (0.5f, 0.3f, 3.0f, 20, 20);

	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size ();
	UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size ();
	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size ();

	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size ();
	UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size ();
	UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size ();

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = box.Indices32.size ();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = grid.Indices32.size ();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = sphere.Indices32.size ();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = cylinder.Indices32.size ();
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

	UINT totalVertexCount = box.Vertices.size () + grid.Vertices.size () + sphere.Vertices.size () + cylinder.Vertices.size ();

	std::vector<Vertex> vertices (totalVertexCount);
	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size (); i++, k++) {
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].TexC = box.Vertices[i].TexC;
	}

	for (size_t i = 0; i < grid.Vertices.size (); i++, k++) {
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Normal = grid.Vertices[i].Normal;
		vertices[k].TexC = grid.Vertices[i].TexC;
	}

	for (size_t i = 0; i < sphere.Vertices.size (); i++, k++) {
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Normal = sphere.Vertices[i].Normal;
		vertices[k].TexC = sphere.Vertices[i].TexC;
	}

	for (size_t i = 0; i < cylinder.Vertices.size (); i++, k++) {
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Normal = cylinder.Vertices[i].Normal;
		vertices[k].TexC = cylinder.Vertices[i].TexC;
	}

	std::vector<std::uint16_t> indices;
	indices.insert (indices.end (), std::begin (box.GetIndices16 ()), std::end (box.GetIndices16 ()));
	indices.insert (indices.end (), std::begin (grid.GetIndices16 ()), std::end (grid.GetIndices16 ()));
	indices.insert (indices.end (), std::begin (sphere.GetIndices16 ()), std::end (sphere.GetIndices16 ()));
	indices.insert (indices.end (), std::begin (cylinder.GetIndices16 ()), std::end (cylinder.GetIndices16 ()));

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
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;

	m_Geometries[geo->Name] = std::move (geo);
}

void BlendDemoApp::BuildDefaultSceneMaterials () {
	auto bricks0 = std::make_unique<Material> ();
	bricks0->Name = "bricks0";
	bricks0->MatCBIndex = 0;
	bricks0->DiffuseSrvHeapIndex = 0;
	bricks0->DiffuseAlbedo = XMFLOAT4 (1.0f, 1.0f, 1.0f, 1.0f);
	bricks0->FresnelR0 = XMFLOAT3 (0.02f, 0.02f, 0.02f);
	bricks0->Roughness = 0.1f;

	auto stone0 = std::make_unique<Material> ();
	stone0->Name = "stone0";
	stone0->MatCBIndex = 1;
	stone0->DiffuseSrvHeapIndex = 1;
	stone0->DiffuseAlbedo = XMFLOAT4 (1.0f, 1.0f, 1.0f, 1.0f);
	stone0->FresnelR0 = XMFLOAT3 (0.05f, 0.05f, 0.05f);
	stone0->Roughness = 0.3f;

	auto tile0 = std::make_unique<Material> ();
	tile0->Name = "tile0";
	tile0->MatCBIndex = 2;
	tile0->DiffuseSrvHeapIndex = 2;
	tile0->DiffuseAlbedo = XMFLOAT4 (1.0f, 1.0f, 1.0f, 1.0f);
	tile0->FresnelR0 = XMFLOAT3 (0.02f, 0.02f, 0.02f);
	tile0->Roughness = 0.3f;

	m_Materials["bricks0"] = std::move (bricks0);
	m_Materials["stone0"] = std::move (stone0);
	m_Materials["tile0"] = std::move (tile0);
}

void BlendDemoApp::BuildDefaultSceneRenderItems () {
	auto boxRitem = std::make_unique<RenderItem> ();
	XMStoreFloat4x4 (&boxRitem->World, XMMatrixScaling (2.0f, 2.0f, 2.0f) * XMMatrixTranslation (0.0f, 1.0f, 0.0f));
	XMStoreFloat4x4 (&boxRitem->TexTransform, XMMatrixScaling (1.0f, 1.0f, 1.0f));
	boxRitem->ObjCBIndex = 0;
	boxRitem->Mat = m_Materials["stone0"].get ();
	boxRitem->Geo = m_Geometries["shape"].get ();
	boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
	m_AllRitems.push_back (std::move (boxRitem));

	auto gridRitem = std::make_unique<RenderItem> ();
	gridRitem->World = MathHelper::Identity4x4 ();
	XMStoreFloat4x4 (&gridRitem->TexTransform, XMMatrixScaling (8.0f, 8.0f, 1.0f));
	gridRitem->ObjCBIndex = 1;
	gridRitem->Mat = m_Materials["tile0"].get ();
	gridRitem->Geo = m_Geometries["shape"].get ();
	gridRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	m_AllRitems.push_back (std::move (gridRitem));

	XMMATRIX brickTexTransform = XMMatrixScaling (1.0f, 1.0f, 1.0f);
	UINT objCBIndex = 2;
	for (int i = 0; i < 5; ++i) {
		auto leftCylRitem = std::make_unique<RenderItem> ();
		auto rightCylRitem = std::make_unique<RenderItem> ();
		auto leftSphereRitem = std::make_unique<RenderItem> ();
		auto rightSphereRitem = std::make_unique<RenderItem> ();

		XMMATRIX leftCylWorld = XMMatrixTranslation (-5.0f, 1.5f, -10.0f + i * 5.0f);
		XMMATRIX rightCylWorld = XMMatrixTranslation (+5.0f, 1.5f, -10.0f + i * 5.0f);

		XMMATRIX leftSphereWorld = XMMatrixTranslation (-5.0f, 3.5f, -10.0f + i * 5.0f);
		XMMATRIX rightSphereWorld = XMMatrixTranslation (+5.0f, 3.5f, -10.0f + i * 5.0f);

		XMStoreFloat4x4 (&leftCylRitem->World, rightCylWorld);
		XMStoreFloat4x4 (&leftCylRitem->TexTransform, brickTexTransform);
		leftCylRitem->ObjCBIndex = objCBIndex++;
		leftCylRitem->Mat = m_Materials["bricks0"].get ();
		leftCylRitem->Geo = m_Geometries["shape"].get ();
		leftCylRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftCylRitem->IndexCount = leftCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		leftCylRitem->StartIndexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		leftCylRitem->BaseVertexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

		XMStoreFloat4x4 (&rightCylRitem->World, leftCylWorld);
		XMStoreFloat4x4 (&rightCylRitem->TexTransform, brickTexTransform);
		rightCylRitem->ObjCBIndex = objCBIndex++;
		rightCylRitem->Mat = m_Materials["bricks0"].get ();
		rightCylRitem->Geo = m_Geometries["shape"].get ();
		rightCylRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightCylRitem->IndexCount = rightCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		rightCylRitem->StartIndexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		rightCylRitem->BaseVertexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

		XMStoreFloat4x4 (&leftSphereRitem->World, leftSphereWorld);
		leftSphereRitem->TexTransform = MathHelper::Identity4x4 ();
		leftSphereRitem->ObjCBIndex = objCBIndex++;
		leftSphereRitem->Mat = m_Materials["stone0"].get ();
		leftSphereRitem->Geo = m_Geometries["shape"].get ();
		leftSphereRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftSphereRitem->IndexCount = leftSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		leftSphereRitem->StartIndexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		leftSphereRitem->BaseVertexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

		XMStoreFloat4x4 (&rightSphereRitem->World, rightSphereWorld);
		rightSphereRitem->TexTransform = MathHelper::Identity4x4 ();
		rightSphereRitem->ObjCBIndex = objCBIndex++;
		rightSphereRitem->Mat = m_Materials["stone0"].get ();
		rightSphereRitem->Geo = m_Geometries["shape"].get ();
		rightSphereRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightSphereRitem->IndexCount = rightSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		rightSphereRitem->StartIndexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		rightSphereRitem->BaseVertexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

		m_AllRitems.push_back (std::move (leftCylRitem));
		m_AllRitems.push_back (std::move (rightCylRitem));
		m_AllRitems.push_back (std::move (leftSphereRitem));
		m_AllRitems.push_back (std::move (rightSphereRitem));
	}

	for (auto& e : m_AllRitems)
		m_RitemLayer[(int)RenderLayer::Opaque].push_back (e.get ());
}

void BlendDemoApp::BuildDefaultSceneDescriptorHeaps () {
	//
	// Create the SRV heap.
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

	auto bricksTex = m_Textures["bricksTex"]->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = bricksTex->GetDesc ().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = bricksTex->GetDesc ().MipLevels;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	m_Device->CreateShaderResourceView (bricksTex.Get (), &srvDesc, hDescriptor);

	hDescriptor.Offset (1, m_CbvSrvUavDescriptorSize);

	auto stoneTex = m_Textures["stoneTex"]->Resource;
	srvDesc.Format = stoneTex->GetDesc ().Format;
	srvDesc.Texture2D.MipLevels = stoneTex->GetDesc ().MipLevels;
	m_Device->CreateShaderResourceView (stoneTex.Get (), &srvDesc, hDescriptor);

	hDescriptor.Offset (1, m_CbvSrvUavDescriptorSize);

	auto tileTex = m_Textures["tileTex"]->Resource;
	srvDesc.Format = tileTex->GetDesc ().Format;
	srvDesc.Texture2D.MipLevels = tileTex->GetDesc ().MipLevels;
	m_Device->CreateShaderResourceView (tileTex.Get (), &srvDesc, hDescriptor);
}

#pragma endregion

void BlendDemoApp::LoadTextures () {
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
	fenceTex->Filename = L"../../../Textures/WireFence.dds";
	ThrowIfFailed (DirectX::CreateDDSTextureFromFile12 (m_Device.Get (),
														m_CmdList.Get (), fenceTex->Filename.c_str (),
														fenceTex->Resource, fenceTex->UploadHeap));

	m_Textures[grassTex->Name] = std::move (grassTex);
	m_Textures[waterTex->Name] = std::move (waterTex);
	m_Textures[fenceTex->Name] = std::move (fenceTex);
}

void BlendDemoApp::BuildScene () {
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData land = geoGen.CreateGrid (160, 160, 50, 50);
	GeometryGenerator::MeshData box = geoGen.CreateBox (8.0f, 8.0f, 8.0f, 3);

	UINT landVertexOffset = 0;
	UINT boxVertexOffset = land.Vertices.size ();

	UINT landIndexOffset = 0;
	UINT boxIndexOffset = land.Indices32.size ();

	UINT totalVertexCount = land.Vertices.size () + box.Vertices.size ();

	std::vector<Vertex> vertices (totalVertexCount);
	int k = 0;
	for (size_t i = 0; i < land.Vertices.size (); ++i, k++) {
		auto& p = land.Vertices[i].Position;
		vertices[k].Pos = p;
		vertices[k].Pos.y = GetHillsHeight (p.x, p.z);
		vertices[k].Normal = GetHillsNormal (p.x, p.z);
		vertices[k].TexC = land.Vertices[i].TexC;
	}

	for (size_t i = 0; i < box.Vertices.size (); ++i, k++) {
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].TexC = box.Vertices[i].TexC;
	}

	std::vector<std::uint16_t> indices;
	indices.insert (indices.end (), std::begin (land.GetIndices16 ()), std::end (land.GetIndices16 ()));
	indices.insert (indices.end (), std::begin (box.GetIndices16 ()), std::end (box.GetIndices16 ()));

	const UINT vbByteSize = (UINT)vertices.size () * sizeof (Vertex);
	const UINT ibByteSize = (UINT)indices.size () * sizeof (std::uint16_t);

	auto geo = std::make_unique<MeshGeometry> ();
	geo->Name = "geo";

	ThrowIfFailed (D3DCreateBlob (vbByteSize, &geo->VertexBufferCPU));
	CopyMemory (geo->VertexBufferCPU->GetBufferPointer (), vertices.data (), vbByteSize);

	ThrowIfFailed (D3DCreateBlob (ibByteSize, &geo->IndexBufferCPU));
	CopyMemory (geo->IndexBufferCPU->GetBufferPointer (), indices.data (), ibByteSize);

	geo->VertexBufferGPU = D3DUtil::CreateDefaultBuffer (m_Device.Get (),
														 m_CmdList.Get (), vertices.data (), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = D3DUtil::CreateDefaultBuffer (m_Device.Get (),
														 m_CmdList.Get (), indices.data (), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof (Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry landSubmesh;
	landSubmesh.IndexCount = (UINT)land.GetIndices16 ().size ();
	landSubmesh.StartIndexLocation = landIndexOffset;
	landSubmesh.BaseVertexLocation = landVertexOffset;

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.GetIndices16 ().size ();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	geo->DrawArgs["land"] = landSubmesh;
	geo->DrawArgs["box"] = boxSubmesh;

	m_Geometries["geo"] = std::move (geo);
}

void BlendDemoApp::BuildWaves () {
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
	geo->Name = "waterGeo";

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

	m_Geometries["waterGeo"] = std::move (geo);
}

void BlendDemoApp::BuildMaterials () {
	auto grass = std::make_unique<Material> ();
	grass->Name = "grass";
	grass->MatCBIndex = 0;
	grass->DiffuseSrvHeapIndex = 0;
	grass->DiffuseAlbedo = XMFLOAT4 (1.0f, 1.0f, 1.0f, 1.0f);
	grass->FresnelR0 = XMFLOAT3 (0.01f, 0.01f, 0.01f);
	grass->Roughness = 0.99f;

	auto water = std::make_unique<Material> ();
	water->Name = "water";
	water->MatCBIndex = 1;
	water->DiffuseSrvHeapIndex = 1;
	water->DiffuseAlbedo = XMFLOAT4 (1.0f, 1.0f, 1.0f, 0.25f);
	water->FresnelR0 = XMFLOAT3 (0.1f, 0.1f, 0.1f);
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

void BlendDemoApp::BuildRenderItems () {
	auto wavesRitem = std::make_unique<RenderItem> ();
	wavesRitem->World = MathHelper::Identity4x4 ();
	XMStoreFloat4x4 (&wavesRitem->TexTransform, XMMatrixScaling (5.0f, 5.0f, 1.0f));
	wavesRitem->ObjCBIndex = 0;
	wavesRitem->Mat = m_Materials["water"].get ();
	wavesRitem->Geo = m_Geometries["waterGeo"].get ();
	wavesRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wavesRitem->IndexCount = wavesRitem->Geo->DrawArgs["water"].IndexCount;
	wavesRitem->StartIndexLocation = wavesRitem->Geo->DrawArgs["water"].StartIndexLocation;
	wavesRitem->BaseVertexLocation = wavesRitem->Geo->DrawArgs["water"].BaseVertexLocation;

	m_WavesRitem = wavesRitem.get ();

	m_RitemLayer[(int)RenderLayer::Transparent].push_back (wavesRitem.get ());

	auto landRitem = std::make_unique<RenderItem> ();
	landRitem->World = MathHelper::Identity4x4 ();
	XMStoreFloat4x4 (&landRitem->TexTransform, XMMatrixScaling (5.0f, 5.0f, 1.0f));
	landRitem->ObjCBIndex = 1;
	landRitem->Mat = m_Materials["grass"].get ();
	landRitem->Geo = m_Geometries["geo"].get ();
	landRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	landRitem->IndexCount = landRitem->Geo->DrawArgs["land"].IndexCount;
	landRitem->StartIndexLocation = landRitem->Geo->DrawArgs["land"].StartIndexLocation;
	landRitem->BaseVertexLocation = landRitem->Geo->DrawArgs["land"].BaseVertexLocation;

	m_RitemLayer[(int)RenderLayer::Opaque].push_back (landRitem.get ());

	auto boxRitem = std::make_unique<RenderItem> ();
	XMStoreFloat4x4 (&boxRitem->World, XMMatrixTranslation (3.0f, 2.0f, -9.0f));
	boxRitem->ObjCBIndex = 2;
	boxRitem->Mat = m_Materials["wirefence"].get ();
	boxRitem->Geo = m_Geometries["geo"].get ();
	boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;

	m_RitemLayer[(int)RenderLayer::AlphaTested].push_back (boxRitem.get ());

	m_AllRitems.push_back (std::move (wavesRitem));
	m_AllRitems.push_back (std::move (landRitem));
	m_AllRitems.push_back (std::move (boxRitem));
}

void BlendDemoApp::BuildDescriptorHeaps () {
	//
	// Create the SRV heap.
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

	// next descriptor
	hDescriptor.Offset (1, m_CbvSrvUavDescriptorSize);

	srvDesc.Format = waterTex->GetDesc ().Format;
	m_Device->CreateShaderResourceView (waterTex.Get (), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset (1, m_CbvSrvUavDescriptorSize);

	srvDesc.Format = fenceTex->GetDesc ().Format;
	m_Device->CreateShaderResourceView (fenceTex.Get (), &srvDesc, hDescriptor);
}

void BlendDemoApp::BuildRootSignature () {
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init (D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	slotRootParameter[0].InitAsDescriptorTable (1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[1].InitAsConstantBufferView (0);
	slotRootParameter[2].InitAsConstantBufferView (1);
	slotRootParameter[3].InitAsConstantBufferView (2);

	auto staticSamplers = GetStaticSamplers ();

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc (_countof (slotRootParameter), slotRootParameter,
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

void BlendDemoApp::BuildShadersAndInputLayout () {
	const D3D_SHADER_MACRO defines[] = {
		"FOG", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO alphaTestDefines[] = {
		"FOG", "1",
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	m_Shaders["standardVS"] = D3DUtil::CompileShader (L"Shaders/Default.hlsl", nullptr, "VS", "vs_5_0");
	m_Shaders["opaquePS"] = D3DUtil::CompileShader (L"Shaders/Default.hlsl", defines, "PS", "ps_5_0");
	m_Shaders["alphaTestedPS"] = D3DUtil::CompileShader (L"Shaders/Default.hlsl", alphaTestDefines, "PS", "ps_5_0");

	m_InputLayout = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};
}

void BlendDemoApp::BuildPSOs () {
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

	//
	// PSO for transparent objects.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = opaquePsoDesc;

	D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
	transparencyBlendDesc.BlendEnable = true;
	transparencyBlendDesc.LogicOpEnable = false;
	transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;			// 源因子 (即正在渲染的)
	transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;	// 目標因子 (即在backbuffer中的)
	transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;				// 運算符
	transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// Exerice 5
	//transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_BLUE | D3D12_COLOR_WRITE_ENABLE_ALPHA;


	transparentPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
	ThrowIfFailed (m_Device->CreateGraphicsPipelineState (&transparentPsoDesc, IID_PPV_ARGS (&m_PSOs["transparent"])));

	//
	// PSO for alpha tested objects.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC alphaTestedPsoDesc = opaquePsoDesc;
	alphaTestedPsoDesc.PS = {
		m_Shaders["alphaTestedPS"]->GetBufferPointer (),
		m_Shaders["alphaTestedPS"]->GetBufferSize ()
	};
	alphaTestedPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	ThrowIfFailed (m_Device->CreateGraphicsPipelineState (&alphaTestedPsoDesc, IID_PPV_ARGS (&m_PSOs["alphaTested"])));
}

void BlendDemoApp::BuildFrameResources () {
	for (int i = 0; i < g_NumFrameResources; i++)
		m_FrameResources.push_back (std::make_unique<FrameResource> (m_Device.Get (), 1, (UINT)m_AllRitems.size (), 
																	 (UINT)m_Materials.size (), m_Waves->VertexCount ()));
}

void BlendDemoApp::OnKeyboardInput (const GameTimer& gt) {
	if (GetAsyncKeyState ('1') & 0x8000)
		m_IsWireFrame = true;
	else
		m_IsWireFrame = false;
}

void BlendDemoApp::UpdateCamera (const GameTimer& gt) {
	m_Eye.x = m_Radius * sinf (m_Phi) * cosf (m_Theta);
	m_Eye.y = m_Radius * cosf (m_Phi);
	m_Eye.z = m_Radius * sinf (m_Phi) * sinf (m_Theta);

	XMVECTOR pos = XMVectorSet (m_Eye.x, m_Eye.y, m_Eye.z, 1.0f);
	XMVECTOR target = XMVectorZero ();
	XMVECTOR up = XMVectorSet (0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH (pos, target, up);
	XMStoreFloat4x4 (&m_View, view);
}

void BlendDemoApp::Update (const GameTimer& gt) {
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

	UpdateWaves (gt);
	AnimateMaterials (gt);
	UpdateObjectCBs (gt);
	UpdateMainPassCB (gt);
	UpdateMaterialCBs (gt);
}

void BlendDemoApp::AnimateMaterials (const GameTimer& gt) {
	auto waterMat = m_Materials["water"].get ();

	float& tu = waterMat->MatTransform (3, 0);
	float& tv = waterMat->MatTransform (3, 1);

	tu += 0.1f * gt.DeltaTime ();
	tv += 0.02f * gt.DeltaTime ();

	if (tu >= 1.0f)
		tu -= 1.0f;

	if (tv >= 1.0f)
		tv -= 1.0f;

	waterMat->MatTransform (3, 0) = tu;
	waterMat->MatTransform (3, 1) = tv;

	// Material has changed, so need to update cbuffer.
	waterMat->NumFrameDirty = g_NumFrameResources;
}

void BlendDemoApp::UpdateWaves (const GameTimer& gt) {
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

		// Derive tex-coords from position by 
		// mapping [-w/2,w/2] --> [0,1]
		v.TexC.x = 0.5f + v.Pos.x / m_Waves->Width ();
		v.TexC.y = 0.5f - v.Pos.z / m_Waves->Depth ();

		currWavesVB->CopyData (i, v);
	}

	// Set the dynamic VB of the wave renderitem to the current frame VB.
	m_WavesRitem->Geo->VertexBufferGPU = currWavesVB->Resource ();
}

void BlendDemoApp::UpdateObjectCBs (const GameTimer& gt) {
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

void BlendDemoApp::UpdateMainPassCB (const GameTimer& gt) {
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
	m_MainPassCB.Lights[0].Strength = {0.9f, 0.9f, 0.8f};
	m_MainPassCB.Lights[1].Direction = {-0.57735f, -0.57735f, 0.57735f};
	m_MainPassCB.Lights[1].Strength = {0.3f, 0.3f, 0.3f};
	m_MainPassCB.Lights[2].Direction = {0.0f, -0.707f, -0.707f};
	m_MainPassCB.Lights[2].Strength = {0.15f, 0.15f, 0.15f};

	auto currPassCB = m_CurrFrameResource->PassCB.get ();
	currPassCB->CopyData (0, m_MainPassCB);
}

void BlendDemoApp::UpdateMaterialCBs (const GameTimer& gt) {
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

void BlendDemoApp::Draw (const GameTimer& gt) {
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

	m_CmdList->ClearRenderTargetView (CurrentBackBufferView (), (float*)&m_MainPassCB.FogColor, 0, nullptr);
	m_CmdList->ClearDepthStencilView (DepthStencilView (), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	m_CmdList->OMSetRenderTargets (1, &CurrentBackBufferView (), true, &DepthStencilView ());

	ID3D12DescriptorHeap* descriptorHeaps[] = {m_SrvDescriptorHeap.Get ()};
	m_CmdList->SetDescriptorHeaps (_countof (descriptorHeaps), descriptorHeaps);

	m_CmdList->SetGraphicsRootSignature (m_RootSignature.Get ());

	auto passCB = m_CurrFrameResource->PassCB->Resource ();
	m_CmdList->SetGraphicsRootConstantBufferView (2, passCB->GetGPUVirtualAddress ());

	m_CmdList->SetPipelineState (m_PSOs["opaque"].Get ());
	DrawRenderItems (m_CmdList.Get (), m_RitemLayer[(int)RenderLayer::Opaque]);

	m_CmdList->SetPipelineState (m_PSOs["alphaTested"].Get ());
	DrawRenderItems (m_CmdList.Get (), m_RitemLayer[(int)RenderLayer::AlphaTested]);

	m_CmdList->SetPipelineState (m_PSOs["transparent"].Get ());
	DrawRenderItems (m_CmdList.Get (), m_RitemLayer[(int)RenderLayer::Transparent]);

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

void BlendDemoApp::DrawRenderItems (ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems) {
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

void BlendDemoApp::OnMouseDown (WPARAM btnState, int x, int y) {
	m_LastMousePos.x = x;
	m_LastMousePos.y = y;

	SetCapture (m_MainWnd);
}

void BlendDemoApp::OnMouseMove (WPARAM btnState, int x, int y) {
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

void BlendDemoApp::OnMouseUp (WPARAM btnState, int x, int y) {
	ReleaseCapture ();
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> BlendDemoApp::GetStaticSamplers () {
	// 應用程序一般只會用到這些采樣器中的一部分
	// 所以就將它們全部提前定義好,並作為根簽名的一部分保留下來

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap (
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

float BlendDemoApp::GetHillsHeight (float x, float z)const {
	return 0.3f * (z * sinf (0.1f * x) + x * cosf (0.1f * z));
}

XMFLOAT3 BlendDemoApp::GetHillsNormal (float x, float z)const {
	// n = (-df/dx, 1, -df/dz)
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
		BlendDemoApp theApp (hInstance);

		if (!theApp.Initialize ())
			return 0;

		return theApp.Run ();
	} catch (DxException& e) {
		MessageBox (nullptr, e.ToString ().c_str (), L"HR Failed", MB_OK);
		return 0;
	}
}