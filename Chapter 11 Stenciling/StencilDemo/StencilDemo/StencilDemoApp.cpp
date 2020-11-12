#include "../../../Common/D3DApp.h"
#include "../../../Common/MathHelper.h"
#include "../../../Common/UploadBuffer.h"
#include "../../../Common/GeometryGenerator.h"
#include "../../../Common/DDSTextureLoader.h"
#include "FrameResource.h"
#include "DirectXTex.h"

#pragma comment (lib,"DirectXTex.lib")

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
	Mirrors,
	Reflected,
	AlphaTested,
	Transparent,
	Shadow,
	Count
};

class StencilDemoApp : public D3DApp {
public:
	StencilDemoApp (HINSTANCE hInstance);
	StencilDemoApp (const StencilDemoApp& rhs) = delete;
	StencilDemoApp& operator=(const StencilDemoApp& rhs) = delete;
	~StencilDemoApp ();

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
	void UpdateReflectedPassCB (const GameTimer& gt);
	void AnimateMaterials (const GameTimer& gt);

	void LoadTextures ();
	void BuildScene ();
	void BuildSkullGeometry ();
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

	RenderItem* m_SkullRitem = nullptr;
	RenderItem* m_ReflectedSkullRitem = nullptr;
	RenderItem* m_ShadowedSkullRitem = nullptr;

	bool m_IsWireFrame = false;

	PassConstants m_MainPassCB;
	PassConstants m_ReflectedPassCB;

	XMFLOAT3 m_SkullTranslation = {0.0f, 1.0f, -5.0f};

	XMFLOAT3 m_Eye = {0.0f, 0.0f, 0.0f};
	XMFLOAT4X4 m_View = MathHelper::Identity4x4 ();
	XMFLOAT4X4 m_Proj = MathHelper::Identity4x4 ();

	float m_Theta = 1.24f * XM_PI;
	float m_Phi = 0.42f * XM_PI;
	float m_Radius = 12.0f;

	POINT m_LastMousePos;

	//
	// Exercise 7
	//
	ComPtr<ID3D12DescriptorHeap> m_BoltSrvDescriptorHeap = nullptr;
	std::vector<Texture> m_BoltTextures;
	int m_BoltIndex = 0;

	//
	// Exercise 11
	//
	RenderItem* m_ReflectedFloorRitem = nullptr;
};

StencilDemoApp::StencilDemoApp (HINSTANCE hInstance) : D3DApp (hInstance) {
	m_MainWndCaption = L"Chapter 11 - StencilDemo";
}

StencilDemoApp::~StencilDemoApp () {
	if (m_Device != nullptr)
		FlushCommandQueue ();
}

void StencilDemoApp::OnResize () {
	D3DApp::OnResize ();

	XMMATRIX P = XMMatrixPerspectiveFovLH (0.25f * MathHelper::PI, AspectRatio (), 1.0f, 1000.0f);
	XMStoreFloat4x4 (&m_Proj, P);
}

bool StencilDemoApp::Initialize () {
	if (!D3DApp::Initialize ())
		return false;

	ThrowIfFailed (m_CmdList->Reset (m_CmdAllocator.Get (), nullptr));
	
	//LoadDefultSceneTextures ();
	//BuildDefaultScene ();
	//BuildDefaultSceneMaterials ();
	//BuildDefaultSceneRenderItems ();
	//BuildDefaultSceneDescriptorHeaps ();

	LoadTextures ();
	BuildScene ();
	BuildSkullGeometry ();
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

void StencilDemoApp::LoadDefultSceneTextures () {
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

void StencilDemoApp::BuildDefaultScene () {
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

void StencilDemoApp::BuildDefaultSceneMaterials () {
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

void StencilDemoApp::BuildDefaultSceneRenderItems () {
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

void StencilDemoApp::BuildDefaultSceneDescriptorHeaps () {
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

void StencilDemoApp::LoadTextures () {
	auto bricksTex = std::make_unique<Texture> ();
	bricksTex->Name = "bricksTex";
	bricksTex->Filename = L"../../../Textures/bricks3.dds";
	ThrowIfFailed (DirectX::CreateDDSTextureFromFile12 (m_Device.Get (),
		m_CmdList.Get (), bricksTex->Filename.c_str (),
		bricksTex->Resource, bricksTex->UploadHeap));

	auto checkboradTex = std::make_unique<Texture> ();
	checkboradTex->Name = "checkboardTex";
	checkboradTex->Filename = L"../../../Textures/checkboard.dds";
	ThrowIfFailed (DirectX::CreateDDSTextureFromFile12 (m_Device.Get (),
		m_CmdList.Get (), checkboradTex->Filename.c_str (),
		checkboradTex->Resource, checkboradTex->UploadHeap));

	auto iceTex = std::make_unique<Texture> ();
	iceTex->Name = "iceTex";
	iceTex->Filename = L"../../../Textures/ice.dds";
	ThrowIfFailed (DirectX::CreateDDSTextureFromFile12 (m_Device.Get (),
		m_CmdList.Get (), iceTex->Filename.c_str (),
		iceTex->Resource, iceTex->UploadHeap));

	auto white1x1Tex = std::make_unique<Texture> ();
	white1x1Tex->Name = "white1x1Tex";
	white1x1Tex->Filename = L"../../../Textures/white1x1.dds";
	ThrowIfFailed (DirectX::CreateDDSTextureFromFile12 (m_Device.Get (),
		m_CmdList.Get (), white1x1Tex->Filename.c_str (),
		white1x1Tex->Resource, white1x1Tex->UploadHeap));

	m_Textures[bricksTex->Name] = std::move (bricksTex);
	m_Textures[checkboradTex->Name] = std::move (checkboradTex);
	m_Textures[iceTex->Name] = std::move (iceTex);
	m_Textures[white1x1Tex->Name] = std::move (white1x1Tex);

	//
	// Exercise 7
	//
	for (int i = 1; i <= 60; i++) {
		Texture boltTex = {};
		boltTex.Name = "boltTex" + (i < 10 ? "0" + std::to_string (i) : std::to_string(i));
		boltTex.Filename = L"BoltAnim/Bolt" + (i < 10 ? L"00" + std::to_wstring (i) : L"0" + std::to_wstring (i)) + L".bmp";

		TexMetadata metadata = {};
		ScratchImage scratchImg = {};
		LoadFromWICFile (boltTex.Filename.c_str (), WIC_FLAGS_NONE, &metadata, scratchImg);
		auto img = scratchImg.GetImage (0, 0, 0);

		D3D12_HEAP_PROPERTIES texHeapProp = {};
		texHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
		texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
		texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
		texHeapProp.CreationNodeMask = 0;
		texHeapProp.VisibleNodeMask = 0;

		D3D12_RESOURCE_DESC resDesc = {};
		resDesc.Format = metadata.format;
		resDesc.Width = metadata.width;
		resDesc.Height = metadata.height;
		resDesc.DepthOrArraySize = metadata.arraySize;
		resDesc.SampleDesc.Count = 1;
		resDesc.SampleDesc.Quality = 0;
		resDesc.MipLevels = metadata.mipLevels;
		resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
		resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		ThrowIfFailed (m_Device->CreateCommittedResource (
			&texHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			IID_PPV_ARGS (&boltTex.Resource)
		));

		boltTex.Resource->WriteToSubresource (
			0,
			nullptr,
			img->pixels,
			img->rowPitch,
			img->slicePitch
		);

		m_BoltTextures.push_back(boltTex);
	}
}

void StencilDemoApp::BuildScene () {
	std::array<Vertex, 20> vertices = {
		// Floor: Observe we tile texture coordinates.
		Vertex (-3.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 0.0f, 4.0f), // 0 
		Vertex (-3.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f),
		Vertex (7.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 4.0f, 0.0f),
		Vertex (7.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 4.0f, 4.0f),

		// Wall: Observe we tile texture coordinates, and that we
		// leave a gap in the middle for the mirror.
		Vertex (-3.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f), // 4
		Vertex (-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex (-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 0.0f),
		Vertex (-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 2.0f),

		Vertex (2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f), // 8 
		Vertex (2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex (7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 0.0f),
		Vertex (7.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 2.0f),

		Vertex (-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f), // 12
		Vertex (-3.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex (7.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 0.0f),
		Vertex (7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 1.0f),

		// Mirror
		Vertex (-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f), // 16
		Vertex (-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex (2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f),
		Vertex (2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f)
	};

	std::array<std::int16_t, 30> indices = {
		// Floor
		0, 1, 2,
		0, 2, 3,

		// Walls
		4, 5, 6,
		4, 6, 7,

		8, 9, 10,
		8, 10, 11,

		12, 13, 14,
		12, 14, 15,

		// Mirror
		16, 17, 18,
		16, 18, 19
	};

	SubmeshGeometry floorSubmesh;
	floorSubmesh.IndexCount = 6;
	floorSubmesh.StartIndexLocation = 0;
	floorSubmesh.BaseVertexLocation = 0;

	SubmeshGeometry wallSubmesh;
	wallSubmesh.IndexCount = 18;
	wallSubmesh.StartIndexLocation = 6;
	wallSubmesh.BaseVertexLocation = 0;

	SubmeshGeometry mirrorSubmesh;
	mirrorSubmesh.IndexCount = 6;
	mirrorSubmesh.StartIndexLocation = 24;
	mirrorSubmesh.BaseVertexLocation = 0;

	const UINT vbByteSize = (UINT)vertices.size () * sizeof (Vertex);
	const UINT ibByteSize = (UINT)indices.size () * sizeof (std::uint16_t);

	auto geo = std::make_unique<MeshGeometry> ();
	geo->Name = "roomGeo";

	ThrowIfFailed (D3DCreateBlob (vbByteSize, &geo->VertexBufferCPU));
	CopyMemory (geo->VertexBufferCPU->GetBufferPointer (), vertices.data (), vbByteSize);

	ThrowIfFailed (D3DCreateBlob (ibByteSize, &geo->IndexBufferCPU));
	CopyMemory (geo->IndexBufferCPU->GetBufferPointer (), indices.data (), ibByteSize);

	geo->VertexBufferGPU = D3DUtil::CreateDefaultBuffer (m_Device.Get (), m_CmdList.Get (),
														 vertices.data (), vbByteSize, geo->VertexBufferUploader);
	geo->IndexBufferGPU = D3DUtil::CreateDefaultBuffer (m_Device.Get (), m_CmdList.Get (),
														 indices.data (), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof (Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["floor"] = floorSubmesh;
	geo->DrawArgs["wall"] = wallSubmesh;
	geo->DrawArgs["mirror"] = mirrorSubmesh;

	m_Geometries[geo->Name] = std::move (geo);

	//
	// Exerise 7
	//
	UINT sliceCount = 20;
	UINT stackCount = 5;
	UINT ringCount = stackCount + 1;

	GeometryGenerator geoGen;
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder (1.25f, 1.25f, 2, sliceCount, stackCount);

	auto cylinderGeo = std::make_unique<MeshGeometry> ();
	cylinderGeo->Name = "cylinderGeo";

	UINT vertexCount = ringCount * (sliceCount + 1);		// don't need top and bottom
	UINT indexCount = cylinder.Indices32.size ();

	std::vector<Vertex> cylinderVertices (vertexCount);
	UINT k = 0;
	for (int i = 0; i < vertexCount; i++, k++) {
		cylinderVertices[k].Pos = cylinder.Vertices[i].Position;
		cylinderVertices[k].Normal = cylinder.Vertices[i].Normal;
		cylinderVertices[k].TexC = cylinder.Vertices[i].TexC;
	}

	std::vector<std::uint16_t> cylinderIndices;
	cylinderIndices.insert (cylinderIndices.end (), std::begin (cylinder.GetIndices16 ()), std::end (cylinder.GetIndices16 ()));

	UINT cylinderVBSize = vertexCount * sizeof (Vertex);
	UINT cylinderIBSize = indexCount * sizeof (uint16_t);

	ThrowIfFailed (D3DCreateBlob (cylinderVBSize, &cylinderGeo->VertexBufferCPU));
	CopyMemory (cylinderGeo->VertexBufferCPU->GetBufferPointer (), cylinderVertices.data (), cylinderVBSize);

	ThrowIfFailed (D3DCreateBlob (cylinderIBSize, &cylinderGeo->IndexBufferCPU));
	CopyMemory (cylinderGeo->IndexBufferCPU->GetBufferPointer (), cylinderIndices.data (), cylinderIBSize);

	cylinderGeo->VertexBufferGPU = D3DUtil::CreateDefaultBuffer (m_Device.Get (),
		m_CmdList.Get (), cylinderVertices.data (), cylinderVBSize, cylinderGeo->VertexBufferUploader);

	cylinderGeo->IndexBufferGPU = D3DUtil::CreateDefaultBuffer (m_Device.Get (),
		m_CmdList.Get (), cylinderIndices.data (), cylinderIBSize, cylinderGeo->IndexBufferUploader);

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = indexCount;
	cylinderSubmesh.StartIndexLocation = 0;
	cylinderSubmesh.BaseVertexLocation = 0;

	cylinderGeo->VertexByteStride = sizeof (Vertex);
	cylinderGeo->VertexBufferByteSize = cylinderVBSize;
	cylinderGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
	cylinderGeo->IndexBufferByteSize = cylinderIBSize;

	cylinderGeo->DrawArgs["cylinder"] = cylinderSubmesh;

	m_Geometries[cylinderGeo->Name] = std::move (cylinderGeo);
}

void StencilDemoApp::BuildSkullGeometry () {
	std::ifstream fin ("../../../Models/skull.txt");

	if (!fin) {
		MessageBox (0, L"../../../Models/skull.txt not found.", 0, 0);
		return;
	}

	UINT vcount = 0;
	UINT tcount = 0;
	std::string ignore;

	fin >> ignore >> vcount;
	fin >> ignore >> tcount;
	fin >> ignore >> ignore >> ignore >> ignore;

	std::vector<Vertex> vertices (vcount);
	for (UINT i = 0; i < vcount; ++i) {
		fin >> vertices[i].Pos.x >> vertices[i].Pos.y >> vertices[i].Pos.z;
		fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;

		// Model does not have texture coordinates, so just zero them out.
		vertices[i].TexC = {0.0f, 0.0f};
	}

	fin >> ignore;
	fin >> ignore;
	fin >> ignore;

	std::vector<std::int32_t> indices (3 * tcount);
	for (UINT i = 0; i < tcount; ++i) {
		fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
	}

	fin.close ();

	//
	// Pack the indices of all the meshes into one index buffer.
	//

	const UINT vbByteSize = (UINT)vertices.size () * sizeof (Vertex);

	const UINT ibByteSize = (UINT)indices.size () * sizeof (std::int32_t);

	auto geo = std::make_unique<MeshGeometry> ();
	geo->Name = "skullGeo";

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
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size ();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["skull"] = submesh;

	m_Geometries[geo->Name] = std::move (geo);
}

void StencilDemoApp::BuildMaterials () {
	auto bricks = std::make_unique<Material> ();
	bricks->Name = "bricks";
	bricks->MatCBIndex = 0;
	bricks->DiffuseSrvHeapIndex = 0;
	bricks->DiffuseAlbedo = XMFLOAT4 (1.0f, 1.0f, 1.0f, 1.0f);
	bricks->FresnelR0 = XMFLOAT3 (0.05f, 0.05f, 0.05f);
	bricks->Roughness = 0.25f;

	auto checkertile = std::make_unique<Material> ();
	checkertile->Name = "checkertile";
	checkertile->MatCBIndex = 1;
	checkertile->DiffuseSrvHeapIndex = 1;
	checkertile->DiffuseAlbedo = XMFLOAT4 (1.0f, 1.0f, 1.0f, 1.0f);
	checkertile->FresnelR0 = XMFLOAT3 (0.07f, 0.07f, 0.07f);
	checkertile->Roughness = 0.3f;

	auto icemirror = std::make_unique<Material> ();
	icemirror->Name = "icemirror";
	icemirror->MatCBIndex = 2;
	icemirror->DiffuseSrvHeapIndex = 2;
	icemirror->DiffuseAlbedo = XMFLOAT4 (1.0f, 1.0f, 1.0f, 0.3f);
	icemirror->FresnelR0 = XMFLOAT3 (0.1f, 0.1f, 0.1f);
	icemirror->Roughness = 0.5f;

	auto skullMat = std::make_unique<Material> ();
	skullMat->Name = "skullMat";
	skullMat->MatCBIndex = 3;
	skullMat->DiffuseSrvHeapIndex = 3;
	skullMat->DiffuseAlbedo = XMFLOAT4 (1.0f, 1.0f, 1.0f, 1.0f);
	skullMat->FresnelR0 = XMFLOAT3 (0.05f, 0.05f, 0.05f);
	skullMat->Roughness = 0.3f;

	auto shadowMat = std::make_unique<Material> ();
	shadowMat->Name = "shadowMat";
	shadowMat->MatCBIndex = 4;
	shadowMat->DiffuseSrvHeapIndex = 3;
	shadowMat->DiffuseAlbedo = XMFLOAT4 (0.0f, 0.0f, 0.0f, 0.5f);
	shadowMat->FresnelR0 = XMFLOAT3 (0.001f, 0.001f, 0.001f);
	shadowMat->Roughness = 0.0f;

	m_Materials["bricks"] = std::move (bricks);
	m_Materials["checkertile"] = std::move (checkertile);
	m_Materials["icemirror"] = std::move (icemirror);
	m_Materials["skullMat"] = std::move (skullMat);
	m_Materials["shadowMat"] = std::move (shadowMat);

	//
	// Exercise 7
	//
	auto boltMat = std::make_unique<Material> ();
	boltMat->Name = "boltMat";
	boltMat->MatCBIndex = 5;
	boltMat->DiffuseSrvHeapIndex = 0;
	boltMat->DiffuseAlbedo = XMFLOAT4 (1.0f, 1.0f, 1.0f, 1.0f);
	boltMat->FresnelR0 = XMFLOAT3 (0.001f, 0.001f, 0.001f);
	boltMat->Roughness = 0.99f;
	m_Materials["boltMat"] = std::move (boltMat);
}

void StencilDemoApp::BuildRenderItems () {
	auto floorRitem = std::make_unique<RenderItem> ();
	floorRitem->World = MathHelper::Identity4x4 ();
	floorRitem->TexTransform = MathHelper::Identity4x4 ();
	floorRitem->ObjCBIndex = 0;
	floorRitem->Mat = m_Materials["checkertile"].get ();
	floorRitem->Geo = m_Geometries["roomGeo"].get ();
	floorRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	floorRitem->IndexCount = floorRitem->Geo->DrawArgs["floor"].IndexCount;
	floorRitem->StartIndexLocation = floorRitem->Geo->DrawArgs["floor"].StartIndexLocation;
	floorRitem->BaseVertexLocation = floorRitem->Geo->DrawArgs["floor"].BaseVertexLocation;
	m_RitemLayer[(int)RenderLayer::Opaque].push_back (floorRitem.get ());

	auto wallsRitem = std::make_unique<RenderItem> ();
	wallsRitem->World = MathHelper::Identity4x4 ();
	wallsRitem->TexTransform = MathHelper::Identity4x4 ();
	wallsRitem->ObjCBIndex = 1;
	wallsRitem->Mat = m_Materials["bricks"].get ();
	wallsRitem->Geo = m_Geometries["roomGeo"].get ();
	wallsRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wallsRitem->IndexCount = wallsRitem->Geo->DrawArgs["wall"].IndexCount;
	wallsRitem->StartIndexLocation = wallsRitem->Geo->DrawArgs["wall"].StartIndexLocation;
	wallsRitem->BaseVertexLocation = wallsRitem->Geo->DrawArgs["wall"].BaseVertexLocation;
	m_RitemLayer[(int)RenderLayer::Opaque].push_back (wallsRitem.get ());

	auto skullRitem = std::make_unique<RenderItem> ();
	skullRitem->World = MathHelper::Identity4x4 ();
	skullRitem->TexTransform = MathHelper::Identity4x4 ();
	skullRitem->ObjCBIndex = 2;
	skullRitem->Mat = m_Materials["skullMat"].get ();
	skullRitem->Geo = m_Geometries["skullGeo"].get ();
	skullRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skullRitem->IndexCount = skullRitem->Geo->DrawArgs["skull"].IndexCount;
	skullRitem->StartIndexLocation = skullRitem->Geo->DrawArgs["skull"].StartIndexLocation;
	skullRitem->BaseVertexLocation = skullRitem->Geo->DrawArgs["skull"].BaseVertexLocation;
	m_SkullRitem = skullRitem.get ();
	m_RitemLayer[(int)RenderLayer::Opaque].push_back (skullRitem.get ());

	// Reflected skull will have different world matrix, so it needs to be its own render item.
	auto reflectedSkullRitem = std::make_unique<RenderItem> ();
	*reflectedSkullRitem = *skullRitem;
	reflectedSkullRitem->ObjCBIndex = 3;
	m_ReflectedSkullRitem = reflectedSkullRitem.get ();
	m_RitemLayer[(int)RenderLayer::Reflected].push_back (reflectedSkullRitem.get ());

	// Shadowed skull will have different world matrix, so it needs to be its own render item.
	auto shadowedSkullRitem = std::make_unique<RenderItem> ();
	*shadowedSkullRitem = *skullRitem;
	shadowedSkullRitem->ObjCBIndex = 4;
	shadowedSkullRitem->Mat = m_Materials["shadowMat"].get ();
	m_ShadowedSkullRitem = shadowedSkullRitem.get ();
	m_RitemLayer[(int)RenderLayer::Shadow].push_back (shadowedSkullRitem.get ());

	auto mirrorRitem = std::make_unique<RenderItem> ();
	mirrorRitem->World = MathHelper::Identity4x4 ();
	mirrorRitem->TexTransform = MathHelper::Identity4x4 ();
	mirrorRitem->ObjCBIndex = 5;
	mirrorRitem->Mat = m_Materials["icemirror"].get ();
	mirrorRitem->Geo = m_Geometries["roomGeo"].get ();
	mirrorRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	mirrorRitem->IndexCount = mirrorRitem->Geo->DrawArgs["mirror"].IndexCount;
	mirrorRitem->StartIndexLocation = mirrorRitem->Geo->DrawArgs["mirror"].StartIndexLocation;
	mirrorRitem->BaseVertexLocation = mirrorRitem->Geo->DrawArgs["mirror"].BaseVertexLocation;
	m_RitemLayer[(int)RenderLayer::Mirrors].push_back (mirrorRitem.get ());
	m_RitemLayer[(int)RenderLayer::Transparent].push_back (mirrorRitem.get ());

	m_AllRitems.push_back (std::move (floorRitem));
	m_AllRitems.push_back (std::move (wallsRitem));
	m_AllRitems.push_back (std::move (skullRitem));
	m_AllRitems.push_back (std::move (reflectedSkullRitem));
	m_AllRitems.push_back (std::move (shadowedSkullRitem));
	m_AllRitems.push_back (std::move (mirrorRitem));

	//
	// Exercise 7
	//
	auto cylinderRitem = std::make_unique<RenderItem> ();
	//XMStoreFloat4x4(&cylinderRitem->World, XMMatrixTranslation (5.0f, 2.0f, -5.0f));
	cylinderRitem->World = MathHelper::Identity4x4 ();
	cylinderRitem->TexTransform = MathHelper::Identity4x4 ();
	cylinderRitem->ObjCBIndex = 6;
	cylinderRitem->Mat = m_Materials["boltMat"].get ();
	cylinderRitem->Geo = m_Geometries["cylinderGeo"].get ();
	cylinderRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	cylinderRitem->IndexCount = cylinderRitem->Geo->DrawArgs["cylinder"].IndexCount;
	cylinderRitem->StartIndexLocation = cylinderRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
	cylinderRitem->BaseVertexLocation = cylinderRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;
	m_RitemLayer[(int)RenderLayer::AlphaTested].push_back (cylinderRitem.get ());
	m_AllRitems.push_back (std::move (cylinderRitem));

	//
	// Exercise 11
	//
	auto reflectedFloorRitem = std::make_unique<RenderItem> ();
	*reflectedFloorRitem = *m_AllRitems[0];
	reflectedFloorRitem->ObjCBIndex = 7;
	m_ReflectedFloorRitem = reflectedFloorRitem.get ();
	m_RitemLayer[(int)RenderLayer::Reflected].push_back (reflectedFloorRitem.get ());
	m_AllRitems.push_back (std::move (reflectedFloorRitem));

	XMVECTOR mirrorPlane = XMVectorSet (0.0f, 0.0f, 1.0f, 0.0f); // xy plane
	XMMATRIX R = XMMatrixReflect (mirrorPlane);
	XMMATRIX floorWorld = XMLoadFloat4x4 (&m_ReflectedFloorRitem->World);
	XMStoreFloat4x4 (&m_ReflectedFloorRitem->World, floorWorld * R);
	m_ReflectedFloorRitem->NumFrameDirty = g_NumFrameResources;
}

void StencilDemoApp::BuildDescriptorHeaps () {
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 4;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed (m_Device->CreateDescriptorHeap (&srvHeapDesc, IID_PPV_ARGS (&m_SrvDescriptorHeap)));

	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor (m_SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart ());

	auto bricksTex = m_Textures["bricksTex"]->Resource;
	auto checkboardTex = m_Textures["checkboardTex"]->Resource;
	auto iceTex = m_Textures["iceTex"]->Resource;
	auto white1x1Tex = m_Textures["white1x1Tex"]->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = bricksTex->GetDesc ().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = -1;
	m_Device->CreateShaderResourceView (bricksTex.Get (), &srvDesc, hDescriptor);

	hDescriptor.Offset (1, m_CbvSrvUavDescriptorSize);

	srvDesc.Format = checkboardTex->GetDesc ().Format;
	m_Device->CreateShaderResourceView (checkboardTex.Get (), &srvDesc, hDescriptor);

	hDescriptor.Offset (1, m_CbvSrvUavDescriptorSize);

	srvDesc.Format = iceTex->GetDesc ().Format;
	m_Device->CreateShaderResourceView (iceTex.Get (), &srvDesc, hDescriptor);

	hDescriptor.Offset (1, m_CbvSrvUavDescriptorSize);

	srvDesc.Format = white1x1Tex->GetDesc ().Format;
	m_Device->CreateShaderResourceView (white1x1Tex.Get (), &srvDesc, hDescriptor);

	//
	// Exercise 7
	//
	srvHeapDesc.NumDescriptors = 60;
	ThrowIfFailed (m_Device->CreateDescriptorHeap (&srvHeapDesc, IID_PPV_ARGS (&m_BoltSrvDescriptorHeap)));

	hDescriptor = m_BoltSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart ();
	for (int i = 0; i < 60; i++) {
		auto boltTex = m_BoltTextures[i].Resource;

		srvDesc.Format = boltTex->GetDesc ().Format;
		m_Device->CreateShaderResourceView (boltTex.Get (), &srvDesc, hDescriptor);

		hDescriptor.Offset (1, m_CbvSrvUavDescriptorSize);
	}
}

void StencilDemoApp::BuildRootSignature () {
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

void StencilDemoApp::BuildShadersAndInputLayout () {
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

void StencilDemoApp::BuildPSOs () {
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
	// PSO for transparent objects;
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = opaquePsoDesc;

	D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
	transparencyBlendDesc.BlendEnable = true;
	transparencyBlendDesc.LogicOpEnable = false;
	transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	transparentPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
	ThrowIfFailed (m_Device->CreateGraphicsPipelineState (&transparentPsoDesc, IID_PPV_ARGS (&m_PSOs["transparent"])));

	//
	// PSO for marking stencil mirrors.
	//
	CD3DX12_BLEND_DESC mirrorBlendState (D3D12_DEFAULT);
	mirrorBlendState.RenderTarget[0].RenderTargetWriteMask = 0;		// disable write to back buffer.

	D3D12_DEPTH_STENCIL_DESC mirrorDSS;
	mirrorDSS.DepthEnable = true;
	mirrorDSS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	mirrorDSS.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	mirrorDSS.StencilEnable = true;
	mirrorDSS.StencilReadMask = 0xff;
	mirrorDSS.StencilWriteMask = 0xff;

	mirrorDSS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	mirrorDSS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	mirrorDSS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	mirrorDSS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	// 我們不渲染背面朝向的多邊形, 因而對這些參數的設置並不關心
	mirrorDSS.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	mirrorDSS.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	mirrorDSS.BackFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	mirrorDSS.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC markMirrorsPsoDesc = opaquePsoDesc;
	markMirrorsPsoDesc.BlendState = mirrorBlendState;
	markMirrorsPsoDesc.DepthStencilState = mirrorDSS;
	ThrowIfFailed (m_Device->CreateGraphicsPipelineState (&markMirrorsPsoDesc, IID_PPV_ARGS (&m_PSOs["markStencilMirrors"])));

	//
	// PSO for stencil reflections.
	//
	D3D12_DEPTH_STENCIL_DESC reflectionsDSS;
	reflectionsDSS.DepthEnable = true;
	reflectionsDSS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	reflectionsDSS.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	reflectionsDSS.StencilEnable = true;
	reflectionsDSS.StencilReadMask = 0xff;
	reflectionsDSS.StencilWriteMask = 0xff;

	reflectionsDSS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	reflectionsDSS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	reflectionsDSS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	reflectionsDSS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
	
	//
	// Exercise 3
	//
	//reflectionsDSS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	reflectionsDSS.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	reflectionsDSS.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	reflectionsDSS.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	reflectionsDSS.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC drawReflectionsPsoDesc = opaquePsoDesc;
	drawReflectionsPsoDesc.DepthStencilState = reflectionsDSS;

	//
	// Exerise 6
	//
	//drawReflectionsPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;

	drawReflectionsPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	drawReflectionsPsoDesc.RasterizerState.FrontCounterClockwise = true;
	ThrowIfFailed (m_Device->CreateGraphicsPipelineState (&drawReflectionsPsoDesc, IID_PPV_ARGS (&m_PSOs["drawStencilReflections"])));

	//
	// Exerise 7
	// PSO for alphaTest objects
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC alphaTestDesc = opaquePsoDesc;
	alphaTestDesc.PS = {
		m_Shaders["alphaTestedPS"]->GetBufferPointer (),
		m_Shaders["alphaTestedPS"]->GetBufferSize ()
	};
	alphaTestDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	
	D3D12_RENDER_TARGET_BLEND_DESC additiveBlendDesc;
	additiveBlendDesc.BlendEnable = true;
	additiveBlendDesc.LogicOpEnable = false;
	additiveBlendDesc.SrcBlend = D3D12_BLEND_ONE;
	additiveBlendDesc.DestBlend = D3D12_BLEND_ONE;
	additiveBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	additiveBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	additiveBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	additiveBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	additiveBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	additiveBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	alphaTestDesc.BlendState.RenderTarget[0] = additiveBlendDesc;
	ThrowIfFailed (m_Device->CreateGraphicsPipelineState (&alphaTestDesc, IID_PPV_ARGS (&m_PSOs["alphaTest"])));

	//
	// PSO for shadow objects
	//
	D3D12_DEPTH_STENCIL_DESC shadowDSS;
	shadowDSS.DepthEnable = true;
	shadowDSS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	shadowDSS.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	shadowDSS.StencilEnable = true;
	shadowDSS.StencilReadMask = 0xff;
	shadowDSS.StencilWriteMask = 0xf;;

	shadowDSS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDSS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;

	//
	// Exercise 4
	//
	//shadowDSS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;

	shadowDSS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
	shadowDSS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	// 我們不渲染背面朝向的多邊形, 因而對這些參數的設置並不關心
	shadowDSS.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDSS.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDSS.BackFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
	shadowDSS.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowPsoDesc = transparentPsoDesc;
	shadowPsoDesc.DepthStencilState = shadowDSS;
	ThrowIfFailed (m_Device->CreateGraphicsPipelineState (&shadowPsoDesc, IID_PPV_ARGS (&m_PSOs["shadow"])));

	//
	// Exercise 9
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC zTestPsoDesc = opaquePsoDesc;

	D3D12_RENDER_TARGET_BLEND_DESC zTestBlendDesc = {};
	zTestBlendDesc.BlendEnable = true;
	zTestBlendDesc.LogicOpEnable = false;
	zTestBlendDesc.SrcBlend = D3D12_BLEND_ONE;
	zTestBlendDesc.DestBlend = D3D12_BLEND_ONE;
	zTestBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	zTestBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	zTestBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	zTestBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	zTestBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	zTestBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	zTestPsoDesc.BlendState.RenderTarget[0] = zTestBlendDesc;
	zTestPsoDesc.DepthStencilState.DepthEnable = false;
	ThrowIfFailed (m_Device->CreateGraphicsPipelineState (&zTestPsoDesc, IID_PPV_ARGS (&m_PSOs["zTest"])));
}

void StencilDemoApp::BuildFrameResources () {
	for (int i = 0; i < g_NumFrameResources; i++)
		m_FrameResources.push_back (std::make_unique<FrameResource> (m_Device.Get (), 2, (UINT)m_AllRitems.size (), (UINT)m_Materials.size ()));
}

void StencilDemoApp::OnKeyboardInput (const GameTimer& gt) {
	if (GetAsyncKeyState ('1') & 0x8000)
		m_IsWireFrame = true;
	else
		m_IsWireFrame = false;

	const float dt = gt.DeltaTime ();

	if (GetAsyncKeyState ('A') & 0x8000)
		m_SkullTranslation.x -= 1.0f * dt;

	if (GetAsyncKeyState ('D') & 0x8000)
		m_SkullTranslation.x += 1.0f * dt;

	if (GetAsyncKeyState ('W') & 0x8000)
		m_SkullTranslation.y += 1.0f * dt;

	if (GetAsyncKeyState ('S') & 0x8000)
		m_SkullTranslation.y -= 1.0f * dt;

	// Don't let user move below ground plane.
	m_SkullTranslation.y = MathHelper::Max (m_SkullTranslation.y, 0.0f);

	// Update the new world matrix.
	XMMATRIX skullRotate = XMMatrixRotationY (0.5f * MathHelper::PI);
	XMMATRIX skullScale = XMMatrixScaling (0.45f, 0.45f, 0.45f);
	XMMATRIX skullOffset = XMMatrixTranslation (m_SkullTranslation.x, m_SkullTranslation.y, m_SkullTranslation.z);
	XMMATRIX skullWorld = skullRotate * skullScale * skullOffset;
	XMStoreFloat4x4 (&m_SkullRitem->World, skullWorld);

	// Update reflection world matrix.
	XMVECTOR mirrorPlane = XMVectorSet (0.0f, 0.0f, 1.0f, 0.0f); // xy plane
	XMMATRIX R = XMMatrixReflect (mirrorPlane);
	XMStoreFloat4x4 (&m_ReflectedSkullRitem->World, skullWorld * R);

	// Update shadow world matrix.
	XMVECTOR shadowPlane = XMVectorSet (0.0f, 1.0f, 0.0f, 0.0f); // xz plane
	XMVECTOR toMainLight = -XMLoadFloat3 (&m_MainPassCB.Lights[0].Direction);
	XMMATRIX S = XMMatrixShadow (shadowPlane, toMainLight);
	//
	// Exercise 12
	//
	XMMATRIX shadowOffsetY = XMMatrixTranslation (0.0f, 0.001f, 0.0f);
	//XMStoreFloat4x4 (&m_ShadowedSkullRitem->World, skullWorld * S);
	XMStoreFloat4x4 (&m_ShadowedSkullRitem->World, skullWorld * S * shadowOffsetY);

	m_SkullRitem->NumFrameDirty = g_NumFrameResources;
	m_ReflectedSkullRitem->NumFrameDirty = g_NumFrameResources;
	m_ShadowedSkullRitem->NumFrameDirty = g_NumFrameResources;
}

void StencilDemoApp::UpdateCamera (const GameTimer& gt) {
	m_Eye.x = m_Radius * sinf (m_Phi) * cosf (m_Theta);
	m_Eye.y = m_Radius * cosf (m_Phi);
	m_Eye.z = m_Radius * sinf (m_Phi) * sinf (m_Theta);

	XMVECTOR pos = XMVectorSet (m_Eye.x, m_Eye.y, m_Eye.z, 1.0f);
	XMVECTOR target = XMVectorZero ();
	XMVECTOR up = XMVectorSet (0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH (pos, target, up);
	XMStoreFloat4x4 (&m_View, view);
}

void StencilDemoApp::Update (const GameTimer& gt) {
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
	UpdateReflectedPassCB (gt);
}

void StencilDemoApp::AnimateMaterials (const GameTimer& gt) {
	static float elapsedTime = 0.0f;
	static float nextTexPerSecond = 1.0f / 60.0f;

	elapsedTime += gt.DeltaTime ();
	if (elapsedTime >= nextTexPerSecond) {
		elapsedTime = 0.0f;
		m_BoltIndex = (m_BoltIndex + 1) % 60;
	}

	auto boltMat = m_Materials["boltMat"].get ();
	boltMat->DiffuseSrvHeapIndex = m_BoltIndex;
	boltMat->NumFrameDirty = g_NumFrameResources;
}

void StencilDemoApp::UpdateObjectCBs (const GameTimer& gt) {
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

void StencilDemoApp::UpdateMainPassCB (const GameTimer& gt) {
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

void StencilDemoApp::UpdateMaterialCBs (const GameTimer& gt) {
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

void StencilDemoApp::UpdateReflectedPassCB (const GameTimer& gt) {
	m_ReflectedPassCB = m_MainPassCB;

	XMVECTOR mirrorPlane = XMVectorSet (0.0f, 0.0f, 1.0f, 0.0f); // xy plane
	XMMATRIX R = XMMatrixReflect (mirrorPlane);

	// Reflect the lighting.
	for (int i = 0; i < 3; ++i) {
		XMVECTOR lightDir = XMLoadFloat3 (&m_MainPassCB.Lights[i].Direction);
		XMVECTOR reflectedLightDir = XMVector3TransformNormal (lightDir, R);
		XMStoreFloat3 (&m_ReflectedPassCB.Lights[i].Direction, reflectedLightDir);
	}

	// Reflected pass stored in index 1
	auto currPassCB = m_CurrFrameResource->PassCB.get ();
	currPassCB->CopyData (1, m_ReflectedPassCB);
}

void StencilDemoApp::Draw (const GameTimer& gt) {
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

	m_CmdList->SetGraphicsRootSignature (m_RootSignature.Get ());

	UINT passCBByteSize = D3DUtil::CalcConstantBufferByteSize (sizeof (PassConstants));
	auto passCB = m_CurrFrameResource->PassCB->Resource ();
	m_CmdList->SetGraphicsRootConstantBufferView (2, passCB->GetGPUVirtualAddress ());

	//
	// Exercise 9
	//
	//m_CmdList->SetPipelineState (m_PSOs["zTest"].Get ());

	DrawRenderItems (m_CmdList.Get (), m_RitemLayer[(int)RenderLayer::Opaque]);

	//
	// Exercise 7
	//
	// Draw alphaTest
	//m_CmdList->SetPipelineState (m_PSOs["alphaTest"].Get ());
	//DrawRenderItems (m_CmdList.Get (), m_RitemLayer[(int)RenderLayer::AlphaTested]);

	// Mark the visible mirror pixels in the stencil buffer with the value 1
	m_CmdList->OMSetStencilRef (1);
	m_CmdList->SetPipelineState (m_PSOs["markStencilMirrors"].Get ());
	DrawRenderItems (m_CmdList.Get (), m_RitemLayer[(int)RenderLayer::Mirrors]);

	// Draw the reflection into the mirror only (only for pixels where the stencil buffer is 1)
	// Note that we must supply a different per-pass constant buffer--one with the lights reflected.
	m_CmdList->SetGraphicsRootConstantBufferView (2, passCB->GetGPUVirtualAddress () + 1 * passCBByteSize);
	m_CmdList->SetPipelineState (m_PSOs["drawStencilReflections"].Get ());
	DrawRenderItems (m_CmdList.Get (), m_RitemLayer[(int)RenderLayer::Reflected]);

	m_CmdList->SetGraphicsRootConstantBufferView (2, passCB->GetGPUVirtualAddress ());
	m_CmdList->OMSetStencilRef (0);

	//// Draw mirror with transparency so reflection blends through.
	m_CmdList->SetPipelineState (m_PSOs["transparent"].Get ());
	DrawRenderItems (m_CmdList.Get (), m_RitemLayer[(int)RenderLayer::Transparent]);

	// Draw shadows
	m_CmdList->SetPipelineState (m_PSOs["shadow"].Get ());
	DrawRenderItems (m_CmdList.Get (), m_RitemLayer[(int)RenderLayer::Shadow]);

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

void StencilDemoApp::DrawRenderItems (ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems) {
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

		ID3D12DescriptorHeap* descriptorHeaps[] = {m_SrvDescriptorHeap.Get ()};
		m_CmdList->SetDescriptorHeaps (_countof (descriptorHeaps), descriptorHeaps);
		CD3DX12_GPU_DESCRIPTOR_HANDLE tex (m_SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart ());
		tex.Offset (ri->Mat->DiffuseSrvHeapIndex, m_CbvSrvUavDescriptorSize);

		if (ri->Geo->Name == "cylinderGeo") {
			ID3D12DescriptorHeap* descriptorHeaps[] = {m_BoltSrvDescriptorHeap.Get ()};
			cmdList->SetDescriptorHeaps (_countof (descriptorHeaps), descriptorHeaps);
			tex = m_BoltSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart ();
			tex.Offset (ri->Mat->DiffuseSrvHeapIndex, m_CbvSrvUavDescriptorSize);
		}

		cmdList->SetGraphicsRootDescriptorTable (0, tex);
		cmdList->SetGraphicsRootConstantBufferView (1, objCBAddress);
		cmdList->SetGraphicsRootConstantBufferView (3, matCBAddress);

		cmdList->DrawIndexedInstanced (ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

void StencilDemoApp::OnMouseDown (WPARAM btnState, int x, int y) {
	m_LastMousePos.x = x;
	m_LastMousePos.y = y;

	SetCapture (m_MainWnd);
}

void StencilDemoApp::OnMouseMove (WPARAM btnState, int x, int y) {
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

void StencilDemoApp::OnMouseUp (WPARAM btnState, int x, int y) {
	ReleaseCapture ();
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> StencilDemoApp::GetStaticSamplers () {
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

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE prevInstance,
					PSTR cmdLine, int showCmd) {
					// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag (_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try {
		StencilDemoApp theApp (hInstance);

		if (!theApp.Initialize ())
			return 0;

		return theApp.Run ();
	} catch (DxException& e) {
		MessageBox (nullptr, e.ToString ().c_str (), L"HR Failed", MB_OK);
		return 0;
	}
}