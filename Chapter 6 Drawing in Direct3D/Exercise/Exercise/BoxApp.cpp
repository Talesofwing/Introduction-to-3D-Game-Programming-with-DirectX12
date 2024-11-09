#include "D3DApp.h"
#include "MathHelper.h"
#include "UploadBuffer.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;



struct Vertex {
	XMFLOAT3 Pos;
	XMFLOAT4 Tangent;

#pragma region Exercise 1

	//XMFLOAT3 Normal;
	//XMFLOAT2 Tex0;
	//XMFLOAT2 Tex1;
	//XMCOLOR Color;

#pragma endregion
};

#pragma region Exercise 10

struct Vertex2 {
	XMFLOAT3 Pos;
	XMCOLOR Color;
};

#pragma endregion


#pragma region Exercise 2

struct VPosData {
	XMFLOAT3 Pos;
};

struct VColorData {
	XMFLOAT4 Color;
};

#pragma endregion

struct ObjectConstants {
	XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();

#pragma region Exercise 16
	XMFLOAT4 PulseColor;
#pragma endregion

#pragma region Exercise 6
	float Time = 0.0f;
#pragma endregion
};

class BoxApp : public D3DApp {
public:
	BoxApp(HINSTANCE hInstance);
	BoxApp(const BoxApp& rhs) = delete;
	BoxApp& operator=(const BoxApp& rhs) = delete;
	~BoxApp();

	virtual bool Initialize() override;

private:
	virtual void OnResize() override;
	virtual void Update(const GameTimer& gt) override;
	virtual void Draw(const GameTimer& gt) override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

	void BuildDescriptorHeaps();
	void BuildConstantBuffers();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildBoxGeometry();
	void BuildPSO();

#pragma region Exercise 4
	void BuildPyramidGeometry();
#pragma endregion

#pragma region Exercise 7
	void BuildBoxAndPyramidGeometry();
#pragma endregion

private:
	std::vector<D3D12_INPUT_ELEMENT_DESC> _inputLayout;
	ComPtr<ID3D12PipelineState> _pso = nullptr;
	ComPtr<ID3D12RootSignature> _rootSignature = nullptr;

	ComPtr<ID3D12DescriptorHeap> _cbvHeap = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> _objectCB = nullptr;

	std::unique_ptr<MeshGeometry> _boxGeo = nullptr;

	ComPtr<ID3DBlob> _vsByteCode = nullptr;
	ComPtr<ID3DBlob> _psByteCode = nullptr;

	XMFLOAT4X4 _world = MathHelper::Identity4x4();
	XMFLOAT4X4 _view = MathHelper::Identity4x4();
	XMFLOAT4X4 _proj = MathHelper::Identity4x4();

	float _theta = 0 * XM_PI;
	float _phi = XM_PIDIV2;
	float _radius = 10.0f;

	POINT _lastMousePos;

#pragma region Exercise 4
	std::unique_ptr<MeshGeometry> _pyramidGeo = nullptr;
#pragma endregion

#pragma region Exercise 7
	std::unique_ptr<MeshGeometry> _multipleGeo = nullptr;
#pragma endregion
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
					PSTR cmdLine, int showCmd) {
	 // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try {
		BoxApp theApp(hInstance);
		if (!theApp.Initialize()) {
			return 0;
		}

		return theApp.Run();
	} catch (DxException& e) {
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}

BoxApp::BoxApp(HINSTANCE hInstance) : D3DApp(hInstance) {
	_mainWndCaption = L"Chapter 6 - Box";
}

BoxApp::~BoxApp() {}

bool BoxApp::Initialize() {
	if (!D3DApp::Initialize())
		return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(_cmdList->Reset(_cmdAllocator.Get(), nullptr));

	BuildDescriptorHeaps();
	BuildConstantBuffers();
	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildBoxGeometry();
	BuildPSO();

#pragma region Exercise 4
	BuildPyramidGeometry();
#pragma endregion

#pragma region Exercise 7
	BuildBoxAndPyramidGeometry();
#pragma endregion

	ThrowIfFailed(_cmdList->Close());
	ID3D12CommandList* cmdLists[] = {_cmdList.Get()};
	_cmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	FlushCommandQueue();

	_boxGeo->DisposeUploaders();

	return true;
}

void BoxApp::BuildDescriptorHeaps() {
	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NumDescriptors = 2;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;
	ThrowIfFailed(_device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&_cbvHeap)));
}

void BoxApp::BuildConstantBuffers() {
	_objectCB = std::make_unique<UploadBuffer<ObjectConstants>>(_device.Get(), 2, true);

	UINT objCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	D3D12_GPU_VIRTUAL_ADDRESS cbAddress = _objectCB->Resource()->GetGPUVirtualAddress();

	int boxCBufIndex = 0;
	cbAddress += boxCBufIndex * objCBByteSize;

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
	cbvDesc.BufferLocation = cbAddress;
	cbvDesc.SizeInBytes = objCBByteSize;

#pragma region Exercise 7
	auto handle = _cbvHeap->GetCPUDescriptorHandleForHeapStart();

	_device->CreateConstantBufferView(&cbvDesc, handle);

	cbAddress += objCBByteSize;
	cbvDesc.BufferLocation = cbAddress;

	handle.ptr += _cbvSrvUavDescriptorSize;
#pragma endregion

	_device->CreateConstantBufferView(&cbvDesc, handle);
}

void BoxApp::BuildRootSignature() {
	CD3DX12_ROOT_PARAMETER slotRootParameter[1];
	CD3DX12_DESCRIPTOR_RANGE cbvTable;
	cbvTable.Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
		1,
		0
	);

	slotRootParameter[0].InitAsDescriptorTable(
		1,
		&cbvTable
	);

	// 根簽名由一組根參數構成
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// 創建僅含一個槽位(該槽位指向一個僅由單個常量緩沖區組成的描述符區域的根簽名
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	auto hr = D3D12SerializeRootSignature(
		&rootSigDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(),
		errorBlob.GetAddressOf()
	);

	if (errorBlob != nullptr) {
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(_device->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&_rootSignature)
	));
}

void BoxApp::BuildShadersAndInputLayout() {
	_vsByteCode = D3DUtil::CompileShader(L"Shaders\\Color.hlsl", nullptr, "VS", "vs_5_0");
	_psByteCode = D3DUtil::CompileShader(L"Shaders\\Color.hlsl", nullptr, "PS", "ps_5_0");

	_inputLayout = {
		{"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},

#pragma region Exercise 1
		/*
			{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		*/
#pragma endregion
	};

#pragma region Exercise 2
	// _inputLayout = {
	//    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	//    {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	// };
#pragma endregion

#pragma region Exercise 10
   // _inputLayout = {
   //	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
   //	{"COLOR", 0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
   //};
#pragma endregion
}

void BoxApp::BuildBoxGeometry() {
	std::array<Vertex, 8> vertices = {
		Vertex({XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White)}),
		Vertex({XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT4(Colors::Black)}),
		Vertex({XMFLOAT3(1.0f,  1.0f, -1.0f), XMFLOAT4(Colors::Red)}),
		Vertex({XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green)}),
		Vertex({XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT4(Colors::Blue)}),
		Vertex({XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT4(Colors::Yellow)}),
		Vertex({XMFLOAT3(1.0f,  1.0f,  1.0f), XMFLOAT4(Colors::Cyan)}),
		Vertex({XMFLOAT3(1.0f, -1.0f,  1.0f), XMFLOAT4(Colors::Magenta)})
	};

	std::array<std::uint16_t, 36> indices = {
		// front face
		0, 1, 2,
		0, 2, 3,
		// back face
		4, 6, 5,
		4, 7, 6,
		// left face
		4, 5, 1,
		4, 1, 0,
		// right face
		3, 2, 6,
		3, 6, 7,
		// top face
		1, 5, 6,
		1, 6, 2,
		// bottom face
		4, 0, 3,
		4, 3, 7
	};

#pragma region Exercise 10
	//std::array<Vertex2, 8> vertices = {
	//	Vertex2 ({XMFLOAT3 (-1.0f, -1.0f, -1.0f), XMCOLOR (Colors::White)}),
	//	Vertex2 ({XMFLOAT3 (-1.0f,  1.0f, -1.0f), XMCOLOR (Colors::Black)}),
	//	Vertex2 ({XMFLOAT3 (1.0f,  1.0f, -1.0f), XMCOLOR (Colors::Red)}),
	//	Vertex2 ({XMFLOAT3 (1.0f, -1.0f, -1.0f), XMCOLOR (Colors::Green)}),
	//	Vertex2 ({XMFLOAT3 (-1.0f, -1.0f,  1.0f), XMCOLOR (Colors::Blue)}),
	//	Vertex2 ({XMFLOAT3 (-1.0f,  1.0f,  1.0f), XMCOLOR (Colors::Yellow)}),
	//	Vertex2 ({XMFLOAT3 (1.0f,  1.0f,  1.0f), XMCOLOR (Colors::Cyan)}),
	//	Vertex2 ({XMFLOAT3 (1.0f, -1.0f,  1.0f), XMCOLOR (Colors::Magenta)})
	//};

	//const UINT vbByteSize = vertices.size () * sizeof (Vertex2);
#pragma endregion
	const size_t vbByteSize = vertices.size() * sizeof(Vertex);
	const size_t ibByteSize = indices.size() * sizeof(std::uint16_t);

	_boxGeo = std::make_unique<MeshGeometry>();
	_boxGeo->Name = "Box";

#pragma region Exercise 2

	std::array<VPosData, 8> verticesPos = {
	VPosData({XMFLOAT3(-1.0f, -1.0f, -1.0f)}),
	VPosData({XMFLOAT3(-1.0f, +1.0f, -1.0f)}),
	VPosData({XMFLOAT3(+1.0f, +1.0f, -1.0f)}),
	VPosData({XMFLOAT3(+1.0f, -1.0f, -1.0f)}),
	VPosData({XMFLOAT3(-1.0f, -1.0f, +1.0f)}),
	VPosData({XMFLOAT3(-1.0f, +1.0f, +1.0f)}),
	VPosData({XMFLOAT3(+1.0f, +1.0f, +1.0f)}),
	VPosData({XMFLOAT3(+1.0f, -1.0f, +1.0f)})
	};

	std::array<VColorData, 8> verticesColor = {
		VColorData({XMFLOAT4(Colors::White)}),
		VColorData({XMFLOAT4(Colors::Black)}),
		VColorData({XMFLOAT4(Colors::Red)}),
		VColorData({XMFLOAT4(Colors::Green)}),
		VColorData({XMFLOAT4(Colors::Blue)}),
		VColorData({XMFLOAT4(Colors::Yellow)}),
		VColorData({XMFLOAT4(Colors::Cyan)}),
		VColorData({XMFLOAT4(Colors::Magenta)}) // 
	};

	const size_t vpbByteSize = verticesPos.size() * sizeof(VPosData);
	const size_t vcbByteSize = verticesColor.size() * sizeof(VColorData);

	ThrowIfFailed(D3DCreateBlob(vpbByteSize, &_boxGeo->VertexPosBufferCPU));
	CopyMemory(_boxGeo->VertexPosBufferCPU->GetBufferPointer(), verticesPos.data(), vpbByteSize);
	ThrowIfFailed(D3DCreateBlob(vcbByteSize, &_boxGeo->VertexColorBufferCPU));
	CopyMemory(_boxGeo->VertexColorBufferCPU->GetBufferPointer(), verticesColor.data(), vcbByteSize);

	_boxGeo->VertexPosBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(),
															   _cmdList.Get(), verticesPos.data(), vpbByteSize, _boxGeo->VertexPosBufferUploader);
	_boxGeo->VertexColorBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(),
																 _cmdList.Get(), verticesColor.data(), vcbByteSize, _boxGeo->VertexColorBufferUploader);

	_boxGeo->VertexPosByteStride = sizeof(VPosData);
	_boxGeo->VertexPosBufferByteSize = vpbByteSize;
	_boxGeo->VertexColorByteStride = sizeof(VColorData);
	_boxGeo->VertexColorBufferByteSize = vcbByteSize;

#pragma endregion

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &_boxGeo->VertexBufferCPU));
	CopyMemory(_boxGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
	ThrowIfFailed(D3DCreateBlob(ibByteSize, &_boxGeo->IndexBufferCPU));
	CopyMemory(_boxGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);


	_boxGeo->VertexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(),
															  _cmdList.Get(), vertices.data(), vbByteSize, _boxGeo->VertexBufferUploader);
	_boxGeo->IndexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(),
															  _cmdList.Get(), indices.data(), ibByteSize, _boxGeo->IndexBufferUploader);

#pragma region Exercise 10
	//_boxGeo->VertexByteStride = sizeof (Vertex2);
	_boxGeo->VertexByteStride = sizeof(Vertex);
	_boxGeo->VertexBufferByteSize = vbByteSize;
#pragma endregion

	_boxGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
	_boxGeo->IndexBufferByteSize = ibByteSize;

	SubMeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	_boxGeo->DrawArgs["Box"] = submesh;
}

#pragma region Exercise 4

void BoxApp::BuildPyramidGeometry() {
	std::array<Vertex, 5> vertices {
		Vertex({XMFLOAT3(0.0f,  5.0f,  0.0f), XMFLOAT4(Colors::Red)}),
		Vertex({XMFLOAT3(2.0f,  0.0f,  2.0f), XMFLOAT4(Colors::Green)}),
		Vertex({XMFLOAT3(2.0f,  0.0f, -2.0f), XMFLOAT4(Colors::Green)}),
		Vertex({XMFLOAT3(-2.0f,  0.0f, -2.0f), XMFLOAT4(Colors::Green)}),
		Vertex({XMFLOAT3(-2.0f,  0.0f,  2.0f), XMFLOAT4(Colors::Green)}),
	};

	std::array<std::uint16_t, 18> indices {
		// front face
		0, 3, 2,
		// back face
		0, 1, 4,
		// left face
		0, 4, 3,
		// right face
		0, 2, 1,
		// bottom face
		2, 3, 4,
		4, 1, 2
	};

	const size_t vbByteSize = vertices.size() * sizeof(Vertex);
	const size_t ibByteSize = indices.size() * sizeof(std::uint16_t);

	_pyramidGeo = std::make_unique<MeshGeometry>();
	_pyramidGeo->Name = "Pyramid";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &_pyramidGeo->VertexBufferCPU));
	CopyMemory(_pyramidGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
	ThrowIfFailed(D3DCreateBlob(ibByteSize, &_pyramidGeo->IndexBufferCPU));
	CopyMemory(_pyramidGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	_pyramidGeo->VertexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(),
																  _cmdList.Get(), vertices.data(), vbByteSize, _pyramidGeo->VertexBufferUploader);
	_pyramidGeo->IndexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(),
																 _cmdList.Get(), indices.data(), ibByteSize, _pyramidGeo->IndexBufferUploader);

	_pyramidGeo->VertexByteStride = sizeof(Vertex);
	_pyramidGeo->VertexBufferByteSize = vbByteSize;

	_pyramidGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
	_pyramidGeo->IndexBufferByteSize = ibByteSize;

	SubMeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	_pyramidGeo->DrawArgs["Pyramid"] = submesh;
}

#pragma endregion

#pragma region Exercise 7

void BoxApp::BuildBoxAndPyramidGeometry() {
	std::array<Vertex, 13> vertices {
		// Box
		Vertex({XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White)}),
		Vertex({XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT4(Colors::Black)}),
		Vertex({XMFLOAT3(1.0f,  1.0f, -1.0f), XMFLOAT4(Colors::Red)}),
		Vertex({XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green)}),
		Vertex({XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT4(Colors::Blue)}),
		Vertex({XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT4(Colors::Yellow)}),
		Vertex({XMFLOAT3(1.0f,  1.0f,  1.0f), XMFLOAT4(Colors::Cyan)}),
		Vertex({XMFLOAT3(1.0f, -1.0f,  1.0f), XMFLOAT4(Colors::Magenta)}),
		// Pyramid
		Vertex({XMFLOAT3(0.0f,  2.0f, -1.0f), XMFLOAT4(Colors::Blue)}),
		Vertex({XMFLOAT3(1.0f,  0.0f, -2.0f), XMFLOAT4(Colors::Green)}),
		Vertex({XMFLOAT3(1.0f,  0.0f,  0.0f), XMFLOAT4(Colors::Red)}),
		Vertex({XMFLOAT3(-1.0f,  0.0f,  0.0f), XMFLOAT4(Colors::Cyan)}),
		Vertex({XMFLOAT3(-1.0f,  0.0f, -2.0f), XMFLOAT4(Colors::Black)}),
	};

	std::array<std::uint16_t, 54> indices {
		// Box
		// front face
		0, 1, 2,
		0, 2, 3,
		// back face
		4, 6, 5,
		4, 7, 6,
		// left face
		4, 5, 1,
		4, 1, 0,
		// right face
		3, 2, 6,
		3, 6, 7,
		// top face
		1, 5, 6,
		1, 6, 2,
		// bottom face
		4, 0, 3,
		4, 3, 7,

		// Pyramid
		// front face
		0, 3, 2,
		// back face
		0, 1, 4,
		// left face
		0, 4, 3,
		// right face
		0, 2, 1,
		// bottom face
		2, 3, 4,
		4, 1, 2
	};

	const size_t vbByteSize = vertices.size() * sizeof(Vertex);
	const size_t ibByteSize = indices.size() * sizeof(std::uint16_t);

	_multipleGeo = std::make_unique<MeshGeometry>();
	_multipleGeo->Name = "Geometry";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &_multipleGeo->VertexBufferCPU));
	CopyMemory(_multipleGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
	ThrowIfFailed(D3DCreateBlob(ibByteSize, &_multipleGeo->IndexBufferCPU));
	CopyMemory(_multipleGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	_multipleGeo->VertexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(),
																   _cmdList.Get(), vertices.data(), vbByteSize, _multipleGeo->VertexBufferUploader);
	_multipleGeo->IndexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(),
																  _cmdList.Get(), indices.data(), ibByteSize, _multipleGeo->IndexBufferUploader);

	_multipleGeo->VertexByteStride = sizeof(Vertex);
	_multipleGeo->VertexBufferByteSize = vbByteSize;

	_multipleGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
	_multipleGeo->IndexBufferByteSize = ibByteSize;

	SubMeshGeometry boxSubMesh;
	boxSubMesh.IndexCount = 36;
	boxSubMesh.StartIndexLocation = 0;
	boxSubMesh.BaseVertexLocation = 0;

	SubMeshGeometry pyramidSubMesh;
	pyramidSubMesh.IndexCount = 18;
	pyramidSubMesh.StartIndexLocation = 36;
	pyramidSubMesh.BaseVertexLocation = 8;

	_multipleGeo->DrawArgs["Box"] = boxSubMesh;
	_multipleGeo->DrawArgs["Pyramid"] = pyramidSubMesh;
}

#pragma endregion

void BoxApp::BuildPSO() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.InputLayout = {_inputLayout.data(), (UINT)_inputLayout.size()};
	psoDesc.pRootSignature = _rootSignature.Get();
	psoDesc.VS = {
		_vsByteCode->GetBufferPointer(),
		_vsByteCode->GetBufferSize()
	};
	psoDesc.PS = {
		_psByteCode->GetBufferPointer(),
		_psByteCode->GetBufferSize()
	};
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

#pragma region Exercise 8 & 9
	// psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	// psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
#pragma endregion

	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState.FrontCounterClockwise = false;

#pragma region Exercise 3
	// psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	// psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
#pragma endregion
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = _backBufferFormat;
	psoDesc.SampleDesc.Count = _4xMsaaState ? 4 : 1;
	psoDesc.SampleDesc.Quality = _4xMsaaState ? (_4xMsaaQuality - 1) : 0;
	psoDesc.DSVFormat = _depthStencilFormat;
	ThrowIfFailed(_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&_pso)));
}

void BoxApp::OnResize() {
	D3DApp::OnResize();

	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::PI, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&_proj, P);
}

void BoxApp::Update(const GameTimer& gt) {
	float x = _radius * sinf(_phi) * cosf(_theta);
	float z = _radius * sinf(_phi) * sinf(_theta);
	float y = _radius * cosf(_phi);

	XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&_view, view);

	XMStoreFloat4x4(&_world, XMMatrixTranslation(2.0f, 0.0f, 0.0f));
	XMMATRIX world = XMLoadFloat4x4(&_world);
	XMMATRIX proj = XMLoadFloat4x4(&_proj);
	XMMATRIX worldViewProj = world * view * proj;

	ObjectConstants objConstants;
	XMStoreFloat4x4(&objConstants.WorldViewProj, worldViewProj);
#pragma region Exercise 6
	objConstants.Time = _timer.TotalTime();
#pragma endregion

#pragma region Exercise 16
	objConstants.PulseColor = XMFLOAT4(Colors::Olive);
#pragma endregion

	_objectCB->CopyData(0, objConstants);

#pragma region Exercise 7
	//XMStoreFloat4x4(&_world, XMMatrixTranslation(0.0f, 0.0f, 3.0f));
	//world = XMLoadFloat4x4(&_world);
	//worldViewProj = world * view * proj;
	//ObjectConstants objConstants2;
	//XMStoreFloat4x4(&objConstants2.WorldViewProj, worldViewProj);
	//objConstants2.Time = _timer.TotalTime();
	//objConstants2.PulseColor = XMFLOAT4(Colors::Olive);
	//_objectCB->CopyData(1, objConstants2);
#pragma endregion
}

void BoxApp::Draw(const GameTimer& gt) {
	ThrowIfFailed(_cmdAllocator->Reset());

	ThrowIfFailed(_cmdList->Reset(_cmdAllocator.Get(), _pso.Get()));

	_cmdList->RSSetViewports(1, &_viewport);
	_cmdList->RSSetScissorRects(1, &_scissorRect);

	_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
							  D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET
	));

	_cmdList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	_cmdList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	_cmdList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	_cmdList->IASetVertexBuffers(0, 1, &_boxGeo->VertexBufferView());
	_cmdList->IASetIndexBuffer(&_boxGeo->IndexBufferView());

#pragma region Exercise 2
	//_cmdList->IASetVertexBuffers(0, 1, &_boxGeo->VertexPosBufferView());
	//_cmdList->IASetVertexBuffers(1, 1, &_boxGeo->VertexColorBufferView());
#pragma endregion

	ID3D12DescriptorHeap* descriptorHeaps[] = {_cbvHeap.Get()};
	_cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	_cmdList->SetGraphicsRootSignature(_rootSignature.Get());
	_cmdList->SetGraphicsRootDescriptorTable(0, _cbvHeap->GetGPUDescriptorHandleForHeapStart());

#pragma region Exercise 3
	//_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
	//_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINESTRIP);
	//_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
	//_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	//_cmdList->DrawInstanced (8, 1, 0, 0);
#pragma endregion

	//_cmdList->DrawIndexedInstanced (_boxGeo->DrawArgs["Box"].IndexCount, 1, 0, 0, 0);

#pragma region Exercise 4
	//_cmdList->IASetVertexBuffers(0, 1, &_pyramidGeo->VertexBufferView());
	//_cmdList->IASetIndexBuffer(&_pyramidGeo->IndexBufferView());
	//_cmdList->DrawIndexedInstanced(_pyramidGeo->DrawArgs["Pyramid"].IndexCount, 1, 0, 0, 0);
#pragma endregion

	_cmdList->DrawIndexedInstanced(_boxGeo->DrawArgs["Box"].IndexCount, 1, 0, 0, 0);

#pragma region Exercise 7
	//_cmdList->IASetVertexBuffers(0, 1, &_multipleGeo->VertexBufferView());
	//_cmdList->IASetIndexBuffer(&_multipleGeo->IndexBufferView());
	//auto handle = _cbvHeap->GetGPUDescriptorHandleForHeapStart();
	//_cmdList->SetGraphicsRootDescriptorTable(0, handle);
	//_cmdList->DrawIndexedInstanced(_multipleGeo->DrawArgs["Box"].IndexCount, 1, 0, 0, 0);
	//handle.ptr += _cbvSrvUavDescriptorSize;
	//_cmdList->SetGraphicsRootDescriptorTable(0, handle);
	//_cmdList->DrawIndexedInstanced(_multipleGeo->DrawArgs["Pyramid"].IndexCount, 1, _multipleGeo->DrawArgs["Pyramid"].StartIndexLocation, _multipleGeo->DrawArgs["Pyramid"].BaseVertexLocation, 0);
#pragma endregion

	_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
							  D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT
	));

	ThrowIfFailed(_cmdList->Close());

	ID3D12CommandList* cmdLists[] = {_cmdList.Get()};
	_cmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	ThrowIfFailed(_swapChain->Present(0, 0));
	_currentBackBuffer = (_currentBackBuffer + 1) % _swapChainBufferCount;

	FlushCommandQueue();
}

void BoxApp::OnMouseDown(WPARAM btnState, int x, int y) {
	_lastMousePos.x = x;
	_lastMousePos.y = y;

	SetCapture(_mainWnd);
}

void BoxApp::OnMouseUp(WPARAM btnState, int x, int y) {
	ReleaseCapture();
}

void BoxApp::OnMouseMove(WPARAM btnState, int x, int y) {
	if ((btnState & MK_LBUTTON) != 0) {
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float> (x - _lastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float> (y - _lastMousePos.y));

		// Update angles based on input to orbit camera around box.
		_theta += dx;
		_phi += dy;

		// Restrict the angle m_Phi.
		// 0 and 180 not included.
		_phi = MathHelper::Clamp(_phi, 0.1f, MathHelper::PI - 0.1f);
	} else if ((btnState & MK_RBUTTON) != 0) {
		float dx = 0.005f * static_cast<float> (x - _lastMousePos.x);
		float dy = 0.005f * static_cast<float> (y - _lastMousePos.y);

		_radius += dx - dy;

		_radius = MathHelper::Clamp(_radius, 3.0f, 15.0f);
	}

	_lastMousePos.x = x;
	_lastMousePos.y = y;
}
