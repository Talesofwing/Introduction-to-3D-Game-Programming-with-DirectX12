#include "../../../Common/D3DApp.h"
#include "../../../Common/MathHelper.h"
#include "../../../Common/UploadBuffer.h"
#include "../../../Common/GeometryGenerator.h"
#include "FrameResource.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

const int g_NumFrameResources = 3;

// �惦�L�ƈD������Ҫ�������p�����Y���w��
// �����S�Ų�ͬ�đ��ó����������e
struct RenderItem {
	RenderItem () = default;

	// �������w�ֲ����g�����������g��������
	// �����x�����wλ�������g�е�λ�á������Լ���С
	XMFLOAT4X4 World = MathHelper::Identity4x4 ();		// Unity->Transform (Position, Rotation, Scale)

	// ���Ѹ���־ (dirty flag)���ʾ���w�����P�����Ѱl����׃, �@��ζ���҂��˕r��Ҫ���³������_�^
	// ���ÿ��FrameResource�ж���һ�����w�������_�^,�����҂���회�ÿ��FrameResource���M�и���
	// ��,���҂��޸����w�����ĕr��, ������NumFrameDirty = g_NumFrameResources�M���O��
	// �Ķ�ʹÿ�����YԴ���õ�����
	int NumFramesDirty = g_NumFrameResources;

	// ԓ����ָ���GPU�������_�^����춮�ǰ��Ⱦ��е����w�������_�^
	UINT ObjCBIndex = -1;

	// ����Ⱦ헅��c�L�ƵĎ׺��w��ע��,�L��һ���׺��w���ܕ��õ�������Ⱦ�
	MeshGeometry* Geo = nullptr;

	// �AԪ�ؓ�
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced�����ą���
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};

class ShapesApp : public D3DApp {
public:
	ShapesApp (HINSTANCE hInstance);
	ShapesApp (const ShapesApp& rhs) = delete;
	ShapesApp& operator=(const ShapesApp& rhs) = delete;
	~ShapesApp ();

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

	void BuildDescriptorHeaps ();
	void BuildConstantBufferViews ();
	void BuildRootSignature ();
	void BuildShadersAndInputLayout ();
	void BuildShapeGeometry ();
	void BuildPSOs ();
	void BuildFrameResources ();
	void BuildRenderItems ();
	void DrawRenderItems (ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

private:
	std::vector<std::unique_ptr<FrameResource>> m_FrameResources;
	FrameResource* m_CurrFrameResource = nullptr;
	int m_CurrFrameResourceIndex = 0;

	ComPtr<ID3D12RootSignature> m_RootSignature = nullptr;

	ComPtr<ID3D12DescriptorHeap> m_CbvHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> m_Geometries;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> m_Shaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> m_PSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> m_InputLayout;

	// ����������Ⱦ헵�array
	std::vector<std::unique_ptr<RenderItem>> m_AllRitems;

	// ����PSO������Ⱦ�
	std::vector<RenderItem*> m_OpaqueRitems;			// ��͸��

	PassConstants m_MainPassCB;

	UINT m_PassCbvOffset = 0;

	bool m_IsWireframe = false;

	XMFLOAT3 m_Eye = {0.0f, 0.0f, 0.0f};
	XMFLOAT4X4 m_View = MathHelper::Identity4x4 ();
	XMFLOAT4X4 m_Proj = MathHelper::Identity4x4 ();

	float m_Theta = 1.5f * XM_PI;
	float m_Phi = 0.2f * XM_PI;
	float m_Radius = 15.0f;

	POINT m_LastMousePos;
};

ShapesApp::ShapesApp (HINSTANCE hInstance) : D3DApp (hInstance) {
	m_MainWndCaption = L"Chapter 7 - Shapes";
}

ShapesApp::~ShapesApp () {
	if (m_Device != nullptr)
		FlushCommandQueue ();
}

void ShapesApp::OnResize () {
	D3DApp::OnResize ();

	XMMATRIX P = XMMatrixPerspectiveFovLH (0.25f * MathHelper::PI, AspectRatio (), 1.0f, 1000.0f);
	XMStoreFloat4x4 (&m_Proj, P);
}

bool ShapesApp::Initialize () {
	if (!D3DApp::Initialize ())
		return false;

	// Reset the command list to prep for initialization commands
	ThrowIfFailed (m_CmdList->Reset (m_CmdAllocator.Get (), nullptr));

	BuildRootSignature ();
	BuildShadersAndInputLayout ();
	BuildShapeGeometry ();
	BuildRenderItems ();
	BuildFrameResources ();		// ��Ҫ֪���ж��ق�Object (RenderItem)
	BuildDescriptorHeaps ();	// ��Ҫ֪���ж��ق�Object (RenderItem)
	BuildConstantBufferViews ();
	BuildPSOs ();

	// Execute the initialization commands.
	ThrowIfFailed (m_CmdList->Close ());
	ID3D12CommandList* cmdsLists[] = {m_CmdList.Get ()};
	m_CmdQueue->ExecuteCommandLists (_countof (cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue ();

	return true;
}

void ShapesApp::BuildRootSignature () {
	// Object constant
	CD3DX12_DESCRIPTOR_RANGE cbvTable0;
	cbvTable0.Init (D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

	// Pass constant
	CD3DX12_DESCRIPTOR_RANGE cbvTable1;
	cbvTable1.Init (D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[2];

	// Create root CBVs.
	slotRootParameter[0].InitAsDescriptorTable (1, &cbvTable0);
	slotRootParameter[1].InitAsDescriptorTable (1, &cbvTable1);

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc (2, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature (&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf (), errorBlob.GetAddressOf ());

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

void ShapesApp::BuildShadersAndInputLayout () {
	m_Shaders["standardVS"] = D3DUtil::CompileShader (L"Shaders/color.hlsl", nullptr, "VS", "vs_5_1");
	m_Shaders["opaquePS"] = D3DUtil::CompileShader (L"Shaders/color.hlsl", nullptr, "PS", "ps_5_1");

	m_InputLayout = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
}

void ShapesApp::BuildShapeGeometry () {
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox (1.5f, 0.5f, 1.5f, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid (20.0f, 30.0f, 60, 40);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere (0.5f, 20, 20);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder (0.5f, 0.3f, 3.0f, 20, 20);

	//
	// �����еĎ׺��w�������Ͼ���һ�������c/�������_�^��
	// �Դˁ��xÿ���ӾW�񔵓��ھ��_�^����ռ�Ĺ���
	//

	// ���Ͼ���c���_�^��ÿ�����w����cƫ�����M�о���
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size ();
	UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size ();
	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size ();

	// ���Ͼ��������_�^��ÿ�����w����ʼ�����M�о���
	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size ();
	UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size ();
	UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size ();

	// ���x�Ķ���SubmeshGeometry�Y���w�а�������c/�������_�^�Ȳ�ͬ�׺��w���ӾW�񔵓�

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size ();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size ();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size ();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size ();
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

	//
	// ��ȡ���������cԪ��, �ٌ����оW�����c���Mһ����c���_��
	//

	auto totalVertexCount = box.Vertices.size () + grid.Vertices.size () + sphere.Vertices.size () + cylinder.Vertices.size ();

	std::vector<Vertex> vertices (totalVertexCount);

	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size (); ++i, ++k) {
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4 (DirectX::Colors::DarkGreen);
	}

	for (size_t i = 0; i < grid.Vertices.size (); ++i, ++k) {
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4 (DirectX::Colors::ForestGreen);
	}

	for (size_t i = 0; i < sphere.Vertices.size (); ++i, ++k) {
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4 (DirectX::Colors::Crimson);
	}

	for (size_t i = 0; i < cylinder.Vertices.size (); ++i, ++k) {
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4 (DirectX::Colors::SteelBlue);
	}

	std::vector<std::uint16_t> indices;
	indices.insert (indices.end (), std::begin (box.GetIndices16 ()), std::end (box.GetIndices16 ()));
	indices.insert (indices.end (), std::begin (grid.GetIndices16 ()), std::end (grid.GetIndices16 ()));
	indices.insert (indices.end (), std::begin (sphere.GetIndices16 ()), std::end (sphere.GetIndices16 ()));
	indices.insert (indices.end (), std::begin (cylinder.GetIndices16 ()), std::end (cylinder.GetIndices16 ()));

	const UINT vbByteSize = (UINT)vertices.size () * sizeof (Vertex);
	const UINT ibByteSize = (UINT)indices.size () * sizeof (std::uint16_t);

	auto geo = std::make_unique<MeshGeometry> ();
	geo->Name = "shapeGeo";

	ThrowIfFailed (D3DCreateBlob (vbByteSize, &geo->VertexBufferCPU));
	CopyMemory (geo->VertexBufferCPU->GetBufferPointer (), vertices.data (), vbByteSize);

	ThrowIfFailed (D3DCreateBlob (vbByteSize, &geo->IndexBufferCPU));
	CopyMemory (geo->IndexBufferCPU->GetBufferPointer (), indices.data (), ibByteSize);

	geo->VertexBufferGPU = D3DUtil::CreateDefaultBuffer (m_Device.Get (), m_CmdList.Get (), vertices.data (), vbByteSize, geo->VertexBufferUploader);
	geo->IndexBufferGPU = D3DUtil::CreateDefaultBuffer (m_Device.Get (), m_CmdList.Get (), indices.data (), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof (Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;

	m_Geometries[geo->Name] = std::move (geo);
}

void ShapesApp::BuildRenderItems () {
	auto boxRitem = std::make_unique<RenderItem> ();
	XMStoreFloat4x4 (&boxRitem->World, XMMatrixScaling (2.0f, 2.0f, 2.0f) * XMMatrixTranslation (0.0f, 0.5f, 0.0f));
	boxRitem->ObjCBIndex = 0;
	boxRitem->Geo = m_Geometries["shapeGeo"].get ();
	boxRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
	m_AllRitems.push_back (move (boxRitem));

	auto gridRitem = std::make_unique<RenderItem> ();
	gridRitem->World = MathHelper::Identity4x4 ();
	gridRitem->ObjCBIndex = 1;
	gridRitem->Geo = m_Geometries["shapeGeo"].get ();
	gridRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	m_AllRitems.push_back (move (gridRitem));

	UINT objCBIndex = 2;
	for (int i = 0; i < 5; ++i) {
		auto leftCylRitem = std::make_unique<RenderItem> ();
		auto rightCylRitem = std::make_unique<RenderItem> ();
		auto leftSphereRitem = std::make_unique<RenderItem> ();
		auto rightSphereRitem = std::make_unique<RenderItem> ();

		XMMATRIX leftCylWorld = XMMatrixTranslation (-5.0f, 1.5f, -10.0f + i * 5.0f);
		XMMATRIX rightCylWorld = XMMatrixTranslation (5.0f, 1.5f, -10.0f + i * 5.0f);

		XMMATRIX leftSphereWorld = XMMatrixTranslation (-5.0f, 3.5f, -10.0f + i * 5.0f);
		XMMATRIX rightSphereWorld = XMMatrixTranslation (5.0f, 3.5f, -10.0f + i * 5.0f);

		XMStoreFloat4x4 (&leftCylRitem->World, leftCylWorld);
		leftCylRitem->ObjCBIndex = objCBIndex++;
		leftCylRitem->Geo = m_Geometries["shapeGeo"].get ();
		leftCylRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftCylRitem->IndexCount = leftCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		leftCylRitem->StartIndexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		leftCylRitem->BaseVertexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

		XMStoreFloat4x4 (&rightCylRitem->World, rightCylWorld);
		rightCylRitem->ObjCBIndex = objCBIndex++;
		rightCylRitem->Geo = m_Geometries["shapeGeo"].get ();
		rightCylRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightCylRitem->IndexCount = rightCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		rightCylRitem->StartIndexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		rightCylRitem->BaseVertexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

		XMStoreFloat4x4 (&leftSphereRitem->World, leftSphereWorld);
		leftSphereRitem->ObjCBIndex = objCBIndex++;
		leftSphereRitem->Geo = m_Geometries["shapeGeo"].get ();
		leftSphereRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftSphereRitem->IndexCount = leftSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		leftSphereRitem->StartIndexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		leftSphereRitem->BaseVertexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

		XMStoreFloat4x4 (&rightSphereRitem->World, rightSphereWorld);
		rightSphereRitem->ObjCBIndex = objCBIndex++;
		rightSphereRitem->Geo = m_Geometries["shapeGeo"].get ();
		rightSphereRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightSphereRitem->IndexCount = rightSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		rightSphereRitem->StartIndexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		rightSphereRitem->BaseVertexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

		m_AllRitems.push_back (move (leftCylRitem));
		m_AllRitems.push_back (move (rightCylRitem));
		m_AllRitems.push_back (move (leftSphereRitem));
		m_AllRitems.push_back (move (rightSphereRitem));
	}

	for (auto& e : m_AllRitems)
		m_OpaqueRitems.push_back (e.get ());
}

void ShapesApp::BuildFrameResources () {
	for (int i = 0; i < g_NumFrameResources; ++i)
		m_FrameResources.push_back (std::make_unique<FrameResource> (m_Device.Get (), 1, (UINT)m_AllRitems.size ()));
}

void ShapesApp::BuildDescriptorHeaps () {
	UINT objCount = (UINT)m_OpaqueRitems.size ();

	// �҂���Ҫ��ÿ�����YԴ�е�ÿһ�����w������һ��CBV������,
	// �����ݼ{ÿ�����YԴ�е���Ⱦ�^��CBV��+1
	UINT numDescriptors = (objCount + 1) * g_NumFrameResources;

	// ������Ⱦ�^��CBV����ʼƫ�������ڱ�������,�@�������������3��������
	// �������3����������PassConstants��
	m_PassCbvOffset = objCount * g_NumFrameResources;

	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NumDescriptors = numDescriptors;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;
	ThrowIfFailed (m_Device->CreateDescriptorHeap (&cbvHeapDesc, IID_PPV_ARGS (&m_CbvHeap)));
}

void ShapesApp::BuildConstantBufferViews () {
	UINT objCBByteSize = D3DUtil::CalcConstantBufferByteSize (sizeof (ObjectConstants));

	UINT objCount = (UINT)m_OpaqueRitems.size ();

	// Need a CBV descriptor for each object for each frame resource.
	for (int frameIndex = 0; frameIndex < g_NumFrameResources; ++frameIndex) {
		auto objectCB = m_FrameResources[frameIndex]->ObjectCB->Resource ();
		for (UINT i = 0; i < objCount; ++i) {
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = objectCB->GetGPUVirtualAddress ();

			// Offset to the ith object constant buffer in the buffer.
			cbAddress += i * objCBByteSize;

			// Offset to the object cbv in the descriptor heap.
			int heapIndex = frameIndex * objCount + i;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE (m_CbvHeap->GetCPUDescriptorHandleForHeapStart ());
			handle.Offset (heapIndex, m_CbvSrvUavDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = objCBByteSize;

			m_Device->CreateConstantBufferView (&cbvDesc, handle);
		}
	}

	UINT passCBByteSize = D3DUtil::CalcConstantBufferByteSize (sizeof (PassConstants));

	// Last three descriptors are the pass CBVs for each frame resource.
	for (int frameIndex = 0; frameIndex < g_NumFrameResources; ++frameIndex) {
		auto passCB = m_FrameResources[frameIndex]->PassCB->Resource ();
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress ();

		// Offset to the pass cbv in the descriptor heap.
		int heapIndex = m_PassCbvOffset + frameIndex;
		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE (m_CbvHeap->GetCPUDescriptorHandleForHeapStart ());
		handle.Offset (heapIndex, m_CbvSrvUavDescriptorSize);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = cbAddress;
		cbvDesc.SizeInBytes = passCBByteSize;

		m_Device->CreateConstantBufferView (&cbvDesc, handle);
	}
}

void ShapesApp::BuildPSOs () {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	//
	// PSO for opaque objects.
	//
	ZeroMemory (&opaquePsoDesc, sizeof (D3D12_GRAPHICS_PIPELINE_STATE_DESC));
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
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC (D3D12_DEFAULT);
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

void ShapesApp::UpdateCamera (const GameTimer& gt) {
	// Convert Spherical to Cartesian coordinates.
	m_Eye.x = m_Radius * sinf (m_Phi) * cosf (m_Theta);
	m_Eye.y = m_Radius * cosf (m_Phi);
	m_Eye.z = m_Radius * sinf (m_Phi) * sinf (m_Theta);

	// Build the view matrix.
	XMVECTOR pos = XMVectorSet (m_Eye.x, m_Eye.y, m_Eye.z, 1.0f);
	XMVECTOR target = XMVectorZero ();
	XMVECTOR up = XMVectorSet (0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH (pos, target, up);
	XMStoreFloat4x4 (&m_View, view);
}

void ShapesApp::OnKeyboardInput (const GameTimer& gt) {
	if (GetAsyncKeyState (VK_SPACE) & 0x8000)
		m_IsWireframe = true;
	else
		m_IsWireframe = false;
}

void ShapesApp::Update (const GameTimer& gt) {
	UpdateCamera (gt);
	OnKeyboardInput (gt);

	// ѭ�h�����ث@ȡ���YԴѭ�h���M�е�Ԫ��
	m_CurrFrameResourceIndex = (m_CurrFrameResourceIndex + 1) % g_NumFrameResources;
	m_CurrFrameResource = m_FrameResources[m_CurrFrameResourceIndex].get ();

	// GPU���Ƿ��ѽ�������̎��ǰ���YԴ������������?
	// ���߀�]�о���CPU�ȴ�, ֱ��GPU�������Ĉ��ЁK���_�@��Fence�c
	if (m_CurrFrameResource->Fence != 0 && m_Fence->GetCompletedValue () < m_CurrFrameResource->Fence) {
		HANDLE eventHandle = CreateEventEx (nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed (m_Fence->SetEventOnCompletion (m_CurrFrameResource->Fence, eventHandle));
		WaitForSingleObject (eventHandle, INFINITE);
		CloseHandle (eventHandle);
	}

	UpdateObjectCBs (gt);
	UpdateMainPassCB (gt);
}

void ShapesApp::UpdateObjectCBs (const GameTimer& gt) {
	auto currObjectCB = m_CurrFrameResource->ObjectCB.get ();
	for (auto& e : m_AllRitems) {
		// ֻҪ�����l���˸�׃�͵ø��³������_�^�ȵĔ�����
		// ����Ҫ��ÿ�����YԴ���M�и���
		if (e->NumFramesDirty > 0) {
			XMMATRIX world = XMLoadFloat4x4 (&e->World);

			ObjectConstants objConstants;
			XMStoreFloat4x4 (&objConstants.World, XMMatrixTranspose (world));

			currObjectCB->CopyData (e->ObjCBIndex, objConstants);

			// ߀��Ҫ����һ��FrameResource�M�и���
			e->NumFramesDirty--;
		}
	}
}

void ShapesApp::UpdateMainPassCB (const GameTimer& gt) {
	XMMATRIX view = XMLoadFloat4x4 (&m_View);
	XMMATRIX proj = XMLoadFloat4x4 (&m_Proj);
	XMMATRIX viewProj = XMMatrixMultiply (view, proj);

	XMMATRIX invView = XMMatrixInverse (&XMMatrixDeterminant (view), view);
	XMMATRIX invProj = XMMatrixInverse (&XMMatrixDeterminant (proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse (&XMMatrixDeterminant (viewProj), viewProj);

	// �D����, ��hlsl�оͿ���ʹ���о�ꇱ�ʾ
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

void ShapesApp::Draw (const GameTimer& gt) {
	auto cmdListAlloc = m_CurrFrameResource->CmdListAlloc;

	// �����cӛ��������P�ăȴ�
	// ֻ����GPU�������cԓ�ȴ����P�������б�r,���܌��������б�������M������
	ThrowIfFailed (cmdListAlloc->Reset ());

	// ��ͨ�^ExecuteCommandList�����������б���ӵ����������֮��,�҂��Ϳ��Ԍ����M������
	// ���������б������c֮���P�ăȴ�
	if (m_IsWireframe) {
		ThrowIfFailed (m_CmdList->Reset (cmdListAlloc.Get (), m_PSOs["opaque_wireframe"].Get ()));
	} else {
		ThrowIfFailed (m_CmdList->Reset (cmdListAlloc.Get (), m_PSOs["opaque"].Get ()));
	}

	m_CmdList->RSSetViewports (1, &m_Viewport);
	m_CmdList->RSSetScissorRects (1, &m_ScissorRect);

	// �����YԴ����;ָʾ�YԴ��B���D�Q
	m_CmdList->ResourceBarrier (1, &CD3DX12_RESOURCE_BARRIER::Transition (CurrentBackBuffer (), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// �����̨���_�^����Ⱦ��_��
	m_CmdList->ClearRenderTargetView (CurrentBackBufferView (), Colors::LightSteelBlue, 0, nullptr);
	m_CmdList->ClearDepthStencilView (DepthStencilView (), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// ָ��Ҫ��Ⱦ��Ŀ�˾��_��
	m_CmdList->OMSetRenderTargets (1, &CurrentBackBufferView (), true, &DepthStencilView ());

	ID3D12DescriptorHeap* descriptorHeaps[] = {m_CbvHeap.Get ()};
	m_CmdList->SetDescriptorHeaps (_countof (descriptorHeaps), descriptorHeaps);

	m_CmdList->SetGraphicsRootSignature (m_RootSignature.Get ());

	int passCbvIndex = m_PassCbvOffset + m_CurrFrameResourceIndex;
	auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE (m_CbvHeap->GetGPUDescriptorHandleForHeapStart ());
	passCbvHandle.Offset (passCbvIndex, m_CbvSrvUavDescriptorSize);
	m_CmdList->SetGraphicsRootDescriptorTable (1, passCbvHandle);

	DrawRenderItems (m_CmdList.Get (), m_OpaqueRitems);

	// �����YԴ����;ָʾ�YԴ��B���D�Q
	m_CmdList->ResourceBarrier (1, &CD3DX12_RESOURCE_BARRIER::Transition (CurrentBackBuffer (), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// ��������ӛ�
	ThrowIfFailed (m_CmdList->Close ());

	// �������б���뵽�����������춈���
	ID3D12CommandList* cmdsLists[] = {m_CmdList.Get ()};
	m_CmdQueue->ExecuteCommandLists (_countof (cmdsLists), cmdsLists);

	// ���Qǰ��̨���_�^
	ThrowIfFailed (m_SwapChain->Present (0, 0));
	m_CurrentBackBuffer = (m_CurrentBackBuffer + 1) % m_SwapChainBufferCount;

	// ���Ӈ���ֵ, ��֮ǰ�������ӛ���ˇ����c��
	m_CurrFrameResource->Fence = ++m_CurrentFence;

	// ������������һ�lָ��, ���O���µć����c
	// GPU߀�ڈ����҂���ǰ����������Ђ��������, ����,GPU���������O����
	// �ć����c, �@Ҫ�ȵ���̎����Signal ()����֮ǰ����������
	m_CmdQueue->Signal (m_Fence.Get (), m_CurrentFence);
}

void ShapesApp::DrawRenderItems (ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems) {
	UINT objCBByteSize = D3DUtil::CalcConstantBufferByteSize (sizeof (ObjectConstants));

	auto objectCB = m_CurrFrameResource->ObjectCB->Resource ();

	// ���ÿ����Ⱦ헁��f
	for (size_t i = 0; i < ritems.size (); ++i) {
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers (0, 1, &ri->Geo->VertexBufferView ());
		cmdList->IASetIndexBuffer (&ri->Geo->IndexBufferView ());
		cmdList->IASetPrimitiveTopology (ri->PrimitiveType);

		// �����L�Ʈ�ǰ�Ď��YԴ�ͮ�ǰ���w,ƫ�Ƶ����������Ќ�����CBV̎
		// ��̎��obj constants
		UINT cbvIndex = m_CurrFrameResourceIndex * (UINT)m_OpaqueRitems.size () + ri->ObjCBIndex;

		auto cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE (m_CbvHeap->GetGPUDescriptorHandleForHeapStart ());
		cbvHandle.Offset (cbvIndex, m_CbvSrvUavDescriptorSize);

		cmdList->SetGraphicsRootDescriptorTable (0, cbvHandle);
		cmdList->DrawIndexedInstanced (ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

void ShapesApp::OnMouseDown (WPARAM btnState, int x, int y) {
	m_LastMousePos.x = x;
	m_LastMousePos.y = y;

	SetCapture (m_MainWnd);
}

void ShapesApp::OnMouseMove (WPARAM btnState, int x, int y) {
	if ((btnState & MK_LBUTTON) != 0) {
		float dx = XMConvertToRadians (0.25f * static_cast<float> (x - m_LastMousePos.x));
		float dy = XMConvertToRadians (0.25f * static_cast<float> (y - m_LastMousePos.y));

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

void ShapesApp::OnMouseUp (WPARAM btnState, int x, int y) {
	ReleaseCapture ();
}

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int showCmd) {
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag (_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try {
		ShapesApp theApp (hInstance);

		if (!theApp.Initialize ())
			return 0;

		return theApp.Run ();
	}
	catch (DxException& e) {
		MessageBox (nullptr, e.ToString ().c_str (), L"HR Failed", MB_OK);
		return 0;
	}
}