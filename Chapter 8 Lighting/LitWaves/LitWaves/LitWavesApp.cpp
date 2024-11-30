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
	RenderItem() = default;

	XMFLOAT4X4 World = MathHelper::Identity4x4();

	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

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
	LitWavesApp(HINSTANCE hInstance);
	LitWavesApp(const LitWavesApp& rhs) = delete;
	LitWavesApp& operator=(const LitWavesApp& rhs) = delete;
	~LitWavesApp();

	virtual bool Initialize() override;

private:
	virtual void OnResize() override;
	virtual void Update(const GameTimer& gt) override;
	virtual void Draw(const GameTimer& gt) override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

	void OnKeyboardInput(const GameTimer& gt);
	void UpdateCamera(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateWaves(const GameTimer& gt);

	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildPSOs();
	void BuildLandGeometry();
	void BuildWavesGeometryBuffers();
	void BuildMaterials();
	void BuildRenderItems();
	void BuildFrameResources();

	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	float GetHillsHeight(float x, float z)const;
	XMFLOAT3 GetHillsNormal(float x, float z)const;

private:
	std::vector<std::unique_ptr<FrameResource>> _frameResources;
	FrameResource* _currFrameResource = nullptr;
	int _currFrameResourceIndex = 0;

	ComPtr<ID3D12RootSignature> _rootSignature = nullptr;

	std::vector<D3D12_INPUT_ELEMENT_DESC> _inputLayout;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> _geometries;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> _shaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> _psos;
	std::unordered_map<std::string, std::unique_ptr<Material>> _materials;

	std::vector<std::unique_ptr<RenderItem>> _allRitems;
	std::vector<RenderItem*> _ritemLayer[(int)RenderLayer::Count];

	std::unique_ptr<Waves> _waves;
	RenderItem* _wavesRitem = nullptr;

	bool _isWireframe = false;

	PassConstants _mainPassCB;

	XMFLOAT3 _eye = {0.0f, 0.0f, 0.0f};
	XMFLOAT4X4 _view = MathHelper::Identity4x4();
	XMFLOAT4X4 _proj = MathHelper::Identity4x4();

	float _theta = 1.5f * XM_PI;
	float _phi = XM_PIDIV2 - 0.1f;
	float _radius = 50.0f;

	float _sunTheta = 1.25f * XM_PI;
	float _sunPhi = XM_PIDIV4;

	POINT _lastMousePos;
};

LitWavesApp::LitWavesApp(HINSTANCE hInstance) : D3DApp(hInstance) {
	_mainWndCaption = L"Chapter 8 - LitWaves";
}

LitWavesApp::~LitWavesApp() {
	if (_device != nullptr) {
		FlushCommandQueue();
	}
}

void LitWavesApp::OnResize() {
	D3DApp::OnResize();

	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::PI, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&_proj, P);
}

bool LitWavesApp::Initialize() {
	if (!D3DApp::Initialize()) {
		return false;
	}

	ThrowIfFailed(_cmdList->Reset(_cmdAllocator.Get(), nullptr));

	_waves = std::make_unique<Waves>(128, 128, 1.0f, 0.03f, 4.0f, 0.2f);

	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildLandGeometry();
	BuildWavesGeometryBuffers();
	BuildMaterials();
	BuildRenderItems();
	BuildFrameResources();
	BuildPSOs();

	ThrowIfFailed(_cmdList->Close());
	ID3D12CommandList* cmdsLists[] = {_cmdList.Get()};
	_cmdQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	FlushCommandQueue();

	for (auto& geo : _geometries) {
		geo.second->DisposeUploaders();
	}

	return true;
}

void LitWavesApp::BuildRootSignature() {
	CD3DX12_ROOT_PARAMETER slotRootParameter[3];
	slotRootParameter[0].InitAsConstantBufferView(0);
	slotRootParameter[1].InitAsConstantBufferView(1);
	slotRootParameter[2].InitAsConstantBufferView(2);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr) {
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(_device->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(_rootSignature.GetAddressOf())
	));
}

void LitWavesApp::BuildShadersAndInputLayout() {
	_shaders["StandardVS"] = D3DUtil::CompileShader(L"Shaders/Default.hlsl", nullptr, "VS", "vs_5_0");
	_shaders["OpaquePS"] = D3DUtil::CompileShader(L"Shaders/Default.hlsl", nullptr, "PS", "ps_5_0");

	_inputLayout = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
}

void LitWavesApp::BuildPSOs() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	//
	// PSO for opaque objects.
	//
	opaquePsoDesc.InputLayout = {_inputLayout.data(), (UINT)_inputLayout.size()};
	opaquePsoDesc.pRootSignature = _rootSignature.Get();
	opaquePsoDesc.VS = {
		_shaders["StandardVS"]->GetBufferPointer(),
		_shaders["StandardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS = {
		_shaders["OpaquePS"]->GetBufferPointer(),
		_shaders["OpaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = _backBufferFormat;
	opaquePsoDesc.SampleDesc.Count = 1;
	opaquePsoDesc.SampleDesc.Quality = 0;
	opaquePsoDesc.DSVFormat = _depthStencilFormat;
	ThrowIfFailed(_device->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&_psos["Opaque"])));

	//
	// PSO for opaque wireframe objects.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
	opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(_device->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&_psos["Wireframe"])));

	//
	// PSO for MSAA
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC msaaPsoDesc = opaquePsoDesc;
	msaaPsoDesc.SampleDesc.Count = 4;
	msaaPsoDesc.SampleDesc.Quality = _4xMsaaQuality - 1;
	ThrowIfFailed(_device->CreateGraphicsPipelineState(&msaaPsoDesc, IID_PPV_ARGS(&_psos["MSAA"])));
}

void LitWavesApp::BuildLandGeometry() {
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(160.0f, 160.0f, 50, 50);

	UINT gridVertexOffset = 0;

	UINT gridIndexOffset = 0;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = grid.Indices32.size();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;

	UINT totalVertexCount = grid.Vertices.size();

	std::vector<Vertex> vertices(totalVertexCount);
	UINT k = 0;
	for (size_t i = 0; i < grid.Vertices.size(); i++, k++) {
		auto& p = grid.Vertices[i].Position;
		vertices[k].Pos = p;
		vertices[k].Pos.y = GetHillsHeight(p.x, p.z);
		vertices[k].Normal = GetHillsNormal(p.x, p.z);
	}

	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));

	UINT vbSize = totalVertexCount * sizeof(Vertex);
	UINT ibSize = indices.size() * sizeof(uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "Land";

	ThrowIfFailed(D3DCreateBlob(vbSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbSize);

	ThrowIfFailed(D3DCreateBlob(ibSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibSize);

	geo->VertexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(),
														 _cmdList.Get(), vertices.data(), vbSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(),
														_cmdList.Get(), indices.data(), ibSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibSize;

	geo->DrawArgs["Land"] = gridSubmesh;

	_geometries[geo->Name] = std::move(geo);
}

void LitWavesApp::BuildWavesGeometryBuffers() {
	std::vector<std::uint16_t> indices(3 * _waves->TriangleCount()); // 3 indices per face
	assert(_waves->VertexCount() < 0x0000ffff);

	// Iterate over each quad.
	int m = _waves->RowCount();
	int n = _waves->ColumnCount();
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

	UINT vbByteSize = _waves->VertexCount() * sizeof(Vertex);
	UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "Water";

	// Set dynamically.
	geo->VertexBufferCPU = nullptr;
	geo->VertexBufferGPU = nullptr;

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->IndexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(), _cmdList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["Water"] = submesh;

	_geometries["Water"] = std::move(geo);
}

void LitWavesApp::BuildMaterials() {
	auto grass = std::make_unique<Material>();
	grass->Name = "Grass";
	grass->MatCBIndex = 0;
	grass->DiffuseAlbedo = XMFLOAT4(0.2f, 0.6f, 0.2f, 1.0f);
	grass->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
#pragma region Exercise 2
	//grass->Roughness = 1.0f;
#pragma endregion
	grass->Roughness = 0.125f;

	auto water = std::make_unique<Material>();
	water->Name = "Water";
	water->MatCBIndex = 1;
	water->DiffuseAlbedo = XMFLOAT4(0.0f, 0.2f, 0.6f, 1.0f);
	water->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	water->Roughness = 0.0f;

	_materials["Grass"] = std::move(grass);
	_materials["Water"] = std::move(water);
}

void LitWavesApp::BuildRenderItems() {
	auto wavesRitem = std::make_unique<RenderItem>();
	wavesRitem->World = MathHelper::Identity4x4();
	wavesRitem->ObjCBIndex = 0;
	wavesRitem->Mat = _materials["Water"].get();
	wavesRitem->Geo = _geometries["Water"].get();
	wavesRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wavesRitem->IndexCount = wavesRitem->Geo->DrawArgs["Water"].IndexCount;
	wavesRitem->StartIndexLocation = wavesRitem->Geo->DrawArgs["Water"].StartIndexLocation;
	wavesRitem->BaseVertexLocation = wavesRitem->Geo->DrawArgs["Water"].BaseVertexLocation;

	_wavesRitem = wavesRitem.get();

	auto landRitem = std::make_unique<RenderItem>();
	landRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&landRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	landRitem->ObjCBIndex = 1;
	landRitem->Geo = _geometries["Land"].get();
	landRitem->Mat = _materials["Grass"].get();
	landRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	landRitem->IndexCount = landRitem->Geo->DrawArgs["Land"].IndexCount;
	landRitem->StartIndexLocation = landRitem->Geo->DrawArgs["Land"].StartIndexLocation;
	landRitem->BaseVertexLocation = landRitem->Geo->DrawArgs["Land"].BaseVertexLocation;

	_allRitems.push_back(std::move(landRitem));
	_allRitems.push_back(std::move(wavesRitem));

	for (auto& e : _allRitems) {
		_ritemLayer[(int)RenderLayer::Opaque].push_back(e.get());
	}
}

void LitWavesApp::BuildFrameResources() {
	for (int i = 0; i < g_NumFrameResources; i++) {
		_frameResources.push_back(std::make_unique<FrameResource>(_device.Get(), 1, (UINT)_allRitems.size(), (UINT)_materials.size(), _waves->VertexCount()));
	}
}

void LitWavesApp::OnKeyboardInput(const GameTimer& gt) {
	if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
		_isWireframe = true;
	} else {
		_isWireframe = false;
	}

	const float dt = gt.DeltaTime();

	if (GetAsyncKeyState(VK_LEFT) & 0x8000) {
		_sunTheta -= 1.0f * dt;
	}

	if (GetAsyncKeyState(VK_RIGHT) & 0x8000) {
		_sunTheta += 1.0f * dt;
	}

	if (GetAsyncKeyState(VK_UP) & 0x8000) {
		_sunPhi -= 1.0f * dt;
	}

	if (GetAsyncKeyState(VK_DOWN) & 0x8000) {
		_sunPhi += 1.0f * dt;
	}

	_sunPhi = MathHelper::Clamp(_sunPhi, 0.1f, XM_PIDIV2);
}

void LitWavesApp::UpdateCamera(const GameTimer& gt) {
	_eye.x = _radius * sinf(_phi) * cosf(_theta);
	_eye.y = _radius * cosf(_phi);
	_eye.z = _radius * sinf(_phi) * sinf(_theta);

	XMVECTOR pos = XMVectorSet(_eye.x, _eye.y, _eye.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&_view, view);
}

void LitWavesApp::Update(const GameTimer& gt) {
	UpdateCamera(gt);
	OnKeyboardInput(gt);

	_currFrameResourceIndex = (_currFrameResourceIndex + 1) % g_NumFrameResources;
	_currFrameResource = _frameResources[_currFrameResourceIndex].get();

	if (_currFrameResource->Fence != 0 && _fence->GetCompletedValue() < _currFrameResource->Fence) {
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(_fence->SetEventOnCompletion(_currFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	UpdateObjectCBs(gt);
	UpdateMainPassCB(gt);
	UpdateMaterialCBs(gt);
	UpdateWaves(gt);
}

void LitWavesApp::UpdateObjectCBs(const GameTimer& gt) {
	auto currObjectCB = _currFrameResource->ObjectCB.get();
	for (auto& e : _allRitems) {
		if (e->NumFrameDirty > 0) {
			XMMATRIX world = XMLoadFloat4x4(&e->World);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			e->NumFrameDirty--;
		}
	}
}

void LitWavesApp::UpdateMainPassCB(const GameTimer& gt) {
	XMMATRIX view = XMLoadFloat4x4(&_view);
	XMMATRIX proj = XMLoadFloat4x4(&_proj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&_mainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&_mainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&_mainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&_mainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&_mainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&_mainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	_mainPassCB.EyePosW = _eye;
	_mainPassCB.RenderTargetSize = XMFLOAT2((float)_clientWidth, (float)_clientHeight);
	_mainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / _clientWidth, 1.0f / _clientHeight);
	_mainPassCB.NearZ = 1.0f;
	_mainPassCB.FarZ = 1000.0f;
	_mainPassCB.TotalTime = gt.TotalTime();
	_mainPassCB.DeltaTime = gt.DeltaTime();

	_mainPassCB.AmbientLight = {0.25f, 0.25f, 0.35f, 1.0f};

	XMVECTOR lightDir = -MathHelper::SphericalToCartesian(1.0f, _sunTheta, _sunPhi);
	XMStoreFloat3(&_mainPassCB.Lights[0].Direction, lightDir);

	_mainPassCB.Lights[0].Strength = {1.0f, 1.0f, 0.9f};

#pragma region Exercise 1
	//float time = gt.TotalTime();
	//static float s = 0.0f;
	//s = abs(sinf(time * 0.5f));
	//_mainPassCB.Lights[0].Strength = {1.0f * s, 0.0f * s, 0.1f * s};
#pragma endregion

	auto currPassCB = _currFrameResource->PassCB.get();
	currPassCB->CopyData(0, _mainPassCB);
}

void LitWavesApp::UpdateMaterialCBs(const GameTimer& gt) {
	auto currMaterialCB = _currFrameResource->MaterialCB.get();
	for (auto& e : _materials) {
		Material* mat = e.second.get();
		if (mat->NumFrameDirty > 0) {
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConstnats;
			matConstnats.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstnats.FresnelR0 = mat->FresnelR0;
			matConstnats.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matConstnats.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialCB->CopyData(mat->MatCBIndex, matConstnats);

			mat->NumFrameDirty--;
		}
	}
}

void LitWavesApp::UpdateWaves(const GameTimer& gt) {
	// Every quarter second, generate a random wave.
	static float t_base = 0.0f;
	if ((_timer.TotalTime() - t_base) >= 0.25f) {
		t_base += 0.25f;

		int i = MathHelper::Rand(4, _waves->RowCount() - 5);
		int j = MathHelper::Rand(4, _waves->ColumnCount() - 5);

		float r = MathHelper::RandF(0.2f, 0.5f);

		_waves->Disturb(i, j, r);
	}

	// Update the wave simulation.
	_waves->Update(gt.DeltaTime());

	// Update the wave vertex buffer with the new solution.
	auto currWavesVB = _currFrameResource->WavesVB.get();
	for (int i = 0; i < _waves->VertexCount(); ++i) {
		Vertex v;

		v.Pos = _waves->Position(i);
		v.Normal = _waves->Normal(i);

		currWavesVB->CopyData(i, v);
	}

	// Set the dynamic VB of the wave renderitem to the current frame VB.
	_wavesRitem->Geo->VertexBufferGPU = currWavesVB->Resource();
}

void LitWavesApp::Draw(const GameTimer& gt) {
	auto cmdListAlloc = _currFrameResource->CmdListAlloc;

	ThrowIfFailed(cmdListAlloc->Reset());
	ThrowIfFailed(_cmdList->Reset(cmdListAlloc.Get(), nullptr));

	if (_4xMsaaState) {
		_cmdList->SetPipelineState(_psos["MSAA"].Get());

		_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_msaaRenderTargetBuffer.Get(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));

		auto rtvDescriptor = _msaaRtvHeap->GetCPUDescriptorHandleForHeapStart();
		auto dsvDescriptor = _msaaDsvHeap->GetCPUDescriptorHandleForHeapStart();
		_cmdList->ClearRenderTargetView(rtvDescriptor, Colors::LightSteelBlue, 0, nullptr);
		_cmdList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		_cmdList->OMSetRenderTargets(1, &rtvDescriptor, false, &dsvDescriptor);
	} else {
		_cmdList->SetPipelineState(_isWireframe ? _psos["Wireframe"].Get() : _psos["Opaque"].Get());

		_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

		_cmdList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
		_cmdList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		_cmdList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());
	}

	_cmdList->RSSetViewports(1, &_viewport);
	_cmdList->RSSetScissorRects(1, &_scissorRect);

	_cmdList->SetGraphicsRootSignature(_rootSignature.Get());

	auto passCB = _currFrameResource->PassCB->Resource();
	_cmdList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

	DrawRenderItems(_cmdList.Get(), _ritemLayer[(int)RenderLayer::Opaque]);

	if (_4xMsaaState) {
		D3D12_RESOURCE_BARRIER barriers[2] = {
			CD3DX12_RESOURCE_BARRIER::Transition(
				_msaaRenderTargetBuffer.Get(),
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_RESOLVE_SOURCE
			),
			CD3DX12_RESOURCE_BARRIER::Transition(
				CurrentBackBuffer(),
				D3D12_RESOURCE_STATE_PRESENT,
				D3D12_RESOURCE_STATE_RESOLVE_DEST
			)
		};
		_cmdList->ResourceBarrier(2, barriers);

		_cmdList->ResolveSubresource(CurrentBackBuffer(), 0, _msaaRenderTargetBuffer.Get(), 0, _backBufferFormat);

		_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_PRESENT));
	} else {
		_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
	}

	ThrowIfFailed(_cmdList->Close());

	ID3D12CommandList* cmdsLists[] = {_cmdList.Get()};
	_cmdQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	ThrowIfFailed(_swapChain->Present(0, 0));
	_currentBackBuffer = (_currentBackBuffer + 1) % _swapChainBufferCount;

	_currFrameResource->Fence = ++_currentFence;

	_cmdQueue->Signal(_fence.Get(), _currentFence);
}

void LitWavesApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems) {
	UINT objCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto objectCB = _currFrameResource->ObjectCB->Resource();
	auto matCB = _currFrameResource->MaterialCB->Resource();

	for (size_t i = 0; i < ritems.size(); i++) {
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress();
		objCBAddress += ri->ObjCBIndex * objCBByteSize;

		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress();
		matCBAddress += ri->Mat->MatCBIndex * matCBByteSize;

		cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);
		cmdList->SetGraphicsRootConstantBufferView(1, matCBAddress);

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

void LitWavesApp::OnMouseDown(WPARAM btnState, int x, int y) {
	_lastMousePos.x = x;
	_lastMousePos.y = y;

	SetCapture(_mainWnd);
}

void LitWavesApp::OnMouseMove(WPARAM btnState, int x, int y) {
	if ((btnState & MK_LBUTTON) != 0) {
		float dx = XMConvertToRadians(0.25f * static_cast<float>(_lastMousePos.x - x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(_lastMousePos.y - y));

		_theta += dx;
		_phi += dy;

		_phi = MathHelper::Clamp(_phi, 0.1f, MathHelper::PI - 0.1f);
	} else if ((btnState & MK_RBUTTON) != 0) {
		float dx = 0.05f * static_cast<float>(x - _lastMousePos.x);
		float dy = 0.05f * static_cast<float>(y - _lastMousePos.y);

		_radius += dx - dy;

		_radius = MathHelper::Clamp(_radius, 5.0f, 150.0f);
	}

	_lastMousePos.x = x;
	_lastMousePos.y = y;
}

void LitWavesApp::OnMouseUp(WPARAM btnState, int x, int y) {
	ReleaseCapture();
}

float LitWavesApp::GetHillsHeight(float x, float z)const {
	return 0.3f * (z * sinf(0.1f * x) + x * cosf(0.1f * z));
}

XMFLOAT3 LitWavesApp::GetHillsNormal(float x, float z)const {
	// n = (-df/dx, 1, -df/dz)
	XMFLOAT3 n(
		-0.03f * z * cosf(0.1f * x) - 0.3f * cosf(0.1f * z),
		1.0f,
		-0.3f * sinf(0.1f * x) + 0.03f * x * sinf(0.1f * z)
	);

	XMVECTOR unitNormal = XMVector3Normalize(XMLoadFloat3(&n));
	XMStoreFloat3(&n, unitNormal);

	return n;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd) {
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try {
		LitWavesApp theApp(hInstance);

		if (!theApp.Initialize())
			return 0;

		return theApp.Run();
	} catch (DxException& e) {
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}
