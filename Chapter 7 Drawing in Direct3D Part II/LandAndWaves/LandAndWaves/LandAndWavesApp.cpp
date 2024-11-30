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
	LandAndWavesApp(HINSTANCE hInstance);
	LandAndWavesApp(const LandAndWavesApp& rhs) = delete;
	LandAndWavesApp& operator=(const LandAndWavesApp& rhs) = delete;
	~LandAndWavesApp();

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
	void UpdateWaves(const GameTimer& gt);

	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildLandGeometry();
	void BuildWavesGeometry();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildRenderItems();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	float GetHillsHeight(float x, float z)const;
	XMFLOAT3 GetHillsNormal(float x, float z)const;

private:
	std::vector<std::unique_ptr<FrameResource>> _frameResources;
	FrameResource* _currFrameResource = nullptr;
	int _currFrameResourceIndex = 0;

	ComPtr<ID3D12RootSignature> _rootSignature = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> _geometries;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> _shaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> _psos;

	std::vector<D3D12_INPUT_ELEMENT_DESC> _inputLayout;

	std::vector<std::unique_ptr<RenderItem>> _allRitems;

	std::vector<RenderItem*> _ritemLayer[(int)RenderLayer::Count];

	RenderItem* _wavesRitem = nullptr;
	std::unique_ptr<Waves> _waves;

	PassConstants _mainPassCB;

	bool _isWireframe = false;

	XMFLOAT3 _eye = {0.0f, 0.0f, 0.0f};
	XMFLOAT4X4 _view = MathHelper::Identity4x4();
	XMFLOAT4X4 _proj = MathHelper::Identity4x4();

	float _theta = 1.5f * XM_PI;
	float _phi = XM_PIDIV2 - 0.1f;
	float _radius = 50.0f;

	POINT _lastMousePos;
};

LandAndWavesApp::LandAndWavesApp(HINSTANCE hInstance) : D3DApp(hInstance) {
	_mainWndCaption = L"Chapter - 7 - LandAndWaves";
}

LandAndWavesApp::~LandAndWavesApp() {
	if (_device != nullptr) {
		FlushCommandQueue();
	}
}

void LandAndWavesApp::OnResize() {
	D3DApp::OnResize();

	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::PI, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&_proj, P);
}

bool LandAndWavesApp::Initialize() {
	if (!D3DApp::Initialize()) {
		return false;
	}

	ThrowIfFailed(_cmdList->Reset(_cmdAllocator.Get(), nullptr));

	_waves = std::make_unique<Waves>(128, 128, 1.0f, 0.03f, 4.0f, 0.2f);

	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildLandGeometry();
	BuildWavesGeometry();
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

void LandAndWavesApp::BuildRootSignature() {
	CD3DX12_ROOT_PARAMETER slotRootParameter[2];

	slotRootParameter[0].InitAsConstantBufferView(0);
	slotRootParameter[1].InitAsConstantBufferView(1);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

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

void LandAndWavesApp::BuildShadersAndInputLayout() {
	_shaders["StandardVS"] = D3DUtil::CompileShader(L"Shaders/color.hlsl", nullptr, "VS", "vs_5_0");
	_shaders["OpaquePS"] = D3DUtil::CompileShader(L"Shaders/color.hlsl", nullptr, "PS", "ps_5_0");

	_inputLayout = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
}

void LandAndWavesApp::BuildLandGeometry() {
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData land = geoGen.CreateGrid(160.0f, 160.0f, 50, 50);

	std::vector<Vertex> vertices(land.Vertices.size());
	for (size_t i = 0; i < land.Vertices.size(); ++i) {
		auto& p = land.Vertices[i].Position;
		vertices[i].Pos = p;
		vertices[i].Pos.y = GetHillsHeight(p.x, p.z);

		if (vertices[i].Pos.y < -10.0f) {
			vertices[i].Color = XMFLOAT4(1.0f, 0.96f, 0.62f, 1.0f);
		} else if (vertices[i].Pos.y < 5.0f) {
			vertices[i].Color = XMFLOAT4(0.48f, 0.77f, 0.46f, 1.0f);
		} else if (vertices[i].Pos.y < 12.0f) {
			vertices[i].Color = XMFLOAT4(0.1f, 0.48f, 0.19f, 1.0f);
		} else if (vertices[i].Pos.y < 20.0f) {
			vertices[i].Color = XMFLOAT4(0.45f, 0.39f, 0.34f, 1.0f);
		} else {
			vertices[i].Color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		}
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	std::vector<std::uint16_t> indices = land.GetIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "Land";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), vertices.data(), ibByteSize);

	geo->VertexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(), _cmdList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);
	geo->IndexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(), _cmdList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["Land"] = submesh;
	_geometries["Land"] = std::move(geo);
}

void LandAndWavesApp::BuildWavesGeometry() {
	std::vector<std::uint16_t> indices(3 * _waves->TriangleCount());	// 3 indices per face
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

			k += 6;
		}
	}

	UINT vbByteSize = _waves->VertexCount() * sizeof(Vertex);
	UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "Water";

	// set dynamically.
	geo->VertexBufferCPU = nullptr;
	geo->VertexBufferGPU = nullptr;

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->IndexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(), _cmdList.Get(),
		indices.data(), ibByteSize, geo->IndexBufferUploader);

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

void LandAndWavesApp::BuildRenderItems() {
	auto wavesRitem = std::make_unique<RenderItem>();
	wavesRitem->World = MathHelper::Identity4x4();
	wavesRitem->ObjCBIndex = 0;
	wavesRitem->Geo = _geometries["Water"].get();
	wavesRitem->PrimitiveType = D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wavesRitem->IndexCount = wavesRitem->Geo->DrawArgs["Water"].IndexCount;
	wavesRitem->StartIndexLocation = wavesRitem->Geo->DrawArgs["Water"].StartIndexLocation;
	wavesRitem->BaseVertexLocation = wavesRitem->Geo->DrawArgs["Water"].BaseVertexLocation;

	_wavesRitem = wavesRitem.get();

	_ritemLayer[(int)RenderLayer::Opaque].push_back(wavesRitem.get());

	auto landRitem = std::make_unique<RenderItem>();
	landRitem->World = MathHelper::Identity4x4();
	landRitem->ObjCBIndex = 0;
	landRitem->Geo = _geometries["Land"].get();
	landRitem->PrimitiveType = D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	landRitem->IndexCount = landRitem->Geo->DrawArgs["Land"].IndexCount;
	landRitem->StartIndexLocation = landRitem->Geo->DrawArgs["Land"].StartIndexLocation;
	landRitem->BaseVertexLocation = landRitem->Geo->DrawArgs["Land"].BaseVertexLocation;

	_ritemLayer[(int)RenderLayer::Opaque].push_back(landRitem.get());

	_allRitems.push_back(std::move(wavesRitem));
	_allRitems.push_back(std::move(landRitem));
}

void LandAndWavesApp::BuildFrameResources() {
	for (int i = 0; i < g_NumFrameResources; i++) {
		_frameResources.push_back(std::make_unique<FrameResource>(_device.Get(), 1, (UINT)_allRitems.size(), _waves->VertexCount()));
	}
}

void LandAndWavesApp::BuildPSOs() {
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

void LandAndWavesApp::OnKeyboardInput(const GameTimer& gt) {
	if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
		_isWireframe = true;
	} else {
		_isWireframe = false;
	}
}

void LandAndWavesApp::UpdateCamera(const GameTimer& gt) {
	_eye.x = _radius * sinf(_phi) * cosf(_theta);
	_eye.y = _radius * cosf(_phi);
	_eye.z = _radius * sinf(_phi) * sinf(_theta);

	XMVECTOR pos = XMVectorSet(_eye.x, _eye.y, _eye.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&_view, view);
}

void LandAndWavesApp::Update(const GameTimer& gt) {
	OnKeyboardInput(gt);
	UpdateCamera(gt);

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
	UpdateWaves(gt);
}

void LandAndWavesApp::UpdateObjectCBs(const GameTimer& gt) {
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

void LandAndWavesApp::UpdateMainPassCB(const GameTimer& gt) {
	XMMATRIX view = XMLoadFloat4x4(&_view);
	XMMATRIX proj = XMLoadFloat4x4(&_proj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&_mainPassCB.View, view);
	XMStoreFloat4x4(&_mainPassCB.InvView, invView);
	XMStoreFloat4x4(&_mainPassCB.Proj, proj);
	XMStoreFloat4x4(&_mainPassCB.InvProj, invProj);
	XMStoreFloat4x4(&_mainPassCB.ViewProj, viewProj);
	XMStoreFloat4x4(&_mainPassCB.InvViewProj, invViewProj);
	_mainPassCB.EyePosW = _eye;
	_mainPassCB.RenderTargetSize = XMFLOAT2((float)_clientWidth, (float)_clientHeight);
	_mainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / _clientWidth, 1.0f / _clientHeight);
	_mainPassCB.NearZ = 1.0f;
	_mainPassCB.FarZ = 1000.0f;
	_mainPassCB.TotalTime = gt.TotalTime();
	_mainPassCB.DeltaTime = gt.DeltaTime();

	auto currPassCB = _currFrameResource->PassCB.get();
	currPassCB->CopyData(0, _mainPassCB);
}

void LandAndWavesApp::UpdateWaves(const GameTimer& gt) {
	static float t_base = 0.0f;
	if (_timer.TotalTime() - t_base >= 0.25f) {
		t_base += 0.25f;

		int i = MathHelper::Rand(4, _waves->RowCount() - 5);
		int j = MathHelper::Rand(4, _waves->ColumnCount() - 5);

		float r = MathHelper::RandF(0.2f, 0.5f);

		_waves->Disturb(i, j, r);
	}

	_waves->Update(gt.DeltaTime());

	auto currWavesVB = _currFrameResource->WavesVB.get();
	for (int i = 0; i < _waves->VertexCount(); ++i) {
		Vertex v;

		v.Pos = _waves->Position(i);
		v.Color = XMFLOAT4(DirectX::Colors::Blue);

		currWavesVB->CopyData(i, v);
	}

	_wavesRitem->Geo->VertexBufferGPU = currWavesVB->Resource();
}

void LandAndWavesApp::Draw(const GameTimer& gt) {
	auto cmdListAlloc = _currFrameResource->CmdListAlloc;
	ThrowIfFailed(cmdListAlloc->Reset());
	ThrowIfFailed(_cmdList->Reset(cmdListAlloc.Get(), nullptr));

	if (_4xMsaaState) {
		_cmdList->SetPipelineState(_psos["MSAA"].Get());

		_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_msaaRenderTargetBuffer.Get(),
								  D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET
		));

		auto rtvDescriptor = _msaaRtvHeap->GetCPUDescriptorHandleForHeapStart();
		auto dsvDescriptor = _msaaDsvHeap->GetCPUDescriptorHandleForHeapStart();
		_cmdList->ClearRenderTargetView(rtvDescriptor, Colors::LightSteelBlue, 0, nullptr);
		_cmdList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		_cmdList->OMSetRenderTargets(1, &rtvDescriptor, false, &dsvDescriptor);
	} else {
		_cmdList->SetPipelineState(_isWireframe ? _psos["Wireframe"].Get() : _psos["Opaque"].Get());

		_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
								  D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET
		));

		_cmdList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
		_cmdList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		_cmdList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());
	}

	_cmdList->RSSetViewports(1, &_viewport);
	_cmdList->RSSetScissorRects(1, &_scissorRect);

	_cmdList->SetGraphicsRootSignature(_rootSignature.Get());

	auto passCB = _currFrameResource->PassCB->Resource();
	_cmdList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

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

		_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
								  D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_PRESENT
		));
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

void LandAndWavesApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems) {
	UINT objCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	auto objectCB = _currFrameResource->ObjectCB->Resource();

	for (size_t i = 0; i < ritems.size(); i++) {
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress();
		objCBAddress += ri->ObjCBIndex * objCBByteSize;

		cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

float LandAndWavesApp::GetHillsHeight(float x, float z) const {
	return 0.3f * (z * sinf(0.1f * x) + x * cosf(0.1f * z));
}

XMFLOAT3 LandAndWavesApp::GetHillsNormal(float x, float z) const {
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

void LandAndWavesApp::OnMouseDown(WPARAM btnState, int x, int y) {
	_lastMousePos.x = x;
	_lastMousePos.y = y;

	SetCapture(_mainWnd);
}

void LandAndWavesApp::OnMouseMove(WPARAM btnState, int x, int y) {
	if ((btnState & MK_LBUTTON) != 0) {
		float dx = XMConvertToRadians(0.25f * static_cast<float> (x - _lastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float> (y - _lastMousePos.y));

		_theta += dx;
		_phi += dy;

		_phi = MathHelper::Clamp(_phi, 0.1f, MathHelper::PI - 0.1f);
	} else if ((btnState & MK_RBUTTON) != 0) {
		float dx = 0.2f * static_cast<float> (x - _lastMousePos.x);
		float dy = 0.2f * static_cast<float> (y - _lastMousePos.y);

		_radius += dx - dy;

		_radius = MathHelper::Clamp(_radius, 5.0f, 150.0f);
	}

	_lastMousePos.x = x;
	_lastMousePos.y = y;
}

void LandAndWavesApp::OnMouseUp(WPARAM btnState, int x, int y) {
	ReleaseCapture();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd) {

#if defined(DEBUG) | defined(_DEBUG)
	// Enable run-time memory check for debug builds.
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try {
		LandAndWavesApp theApp(hInstance);

		if (!theApp.Initialize()) {
			return 0;
		}

		return theApp.Run();
	} catch (DxException& e) {
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}
