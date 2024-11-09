#include "../../../Common/D3DApp.h"
#include "../../../Common/MathHelper.h"
#include "../../../Common/UploadBuffer.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

struct Vertex {
	XMFLOAT3 Pos;
	XMFLOAT4 Color;
};

struct ObjectConstants {
	XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
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

	void BuildCbvDescriptorHeaps();
	void BuildConstantBuffers();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildBoxGeometry();
	void BuildPSO();

private:
	std::vector<D3D12_INPUT_ELEMENT_DESC> _inputLayout;
	ComPtr<ID3D12PipelineState> _pso = nullptr;
	ComPtr<ID3D12PipelineState> _msaaPso = nullptr;
	ComPtr<ID3D12RootSignature> _rootSignature = nullptr;

	ComPtr<ID3D12DescriptorHeap> _cbvHeap = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> _objectCB = nullptr;

	std::unique_ptr<MeshGeometry> _boxGeo = nullptr;

	ComPtr<ID3DBlob> _vsByteCode = nullptr;
	ComPtr<ID3DBlob> _psByteCode = nullptr;

	XMFLOAT4X4 _world = MathHelper::Identity4x4();
	XMFLOAT4X4 _view = MathHelper::Identity4x4();
	XMFLOAT4X4 _proj = MathHelper::Identity4x4();

	float _theta = 1.5f * XM_PI;
	float _phi = XM_PIDIV4;
	float _radius = 10.0f;

	POINT _lastMousePos;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd) {
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
	if (!D3DApp::Initialize()) {
		return false;
	}

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(_cmdList->Reset(_cmdAllocator.Get(), nullptr));

	BuildCbvDescriptorHeaps();
	BuildConstantBuffers();
	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildBoxGeometry();
	BuildPSO();

	ThrowIfFailed(_cmdList->Close());
	ID3D12CommandList* cmdLists[] = {_cmdList.Get()};
	_cmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	FlushCommandQueue();

	_boxGeo->DisposeUploaders();

	return true;
}

void BoxApp::BuildCbvDescriptorHeaps() {
	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NumDescriptors = 1;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;
	ThrowIfFailed(_device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&_cbvHeap)));
}

void BoxApp::BuildConstantBuffers() {
	_objectCB = std::make_unique<UploadBuffer<ObjectConstants>>(_device.Get(), 1, true);

	UINT objCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	D3D12_GPU_VIRTUAL_ADDRESS cbAddress = _objectCB->Resource()->GetGPUVirtualAddress();

	int boxCBufIndex = 0;
	cbAddress += boxCBufIndex * objCBByteSize;

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
	cbvDesc.BufferLocation = cbAddress;
	cbvDesc.SizeInBytes = objCBByteSize;

	_device->CreateConstantBufferView(&cbvDesc, _cbvHeap->GetCPUDescriptorHandleForHeapStart());
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

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

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
	//_vsByteCode = D3DUtil::CompileShader(L"Shaders\\Color.hlsl", nullptr, "VS", "vs_5_0");
	// _psByteCode = D3DUtil::CompileShader(L"Shaders\\Color.hlsl", nullptr, "PS", "ps_5_0");
	_vsByteCode = D3DUtil::LoadBinary(L"Shaders\\color_vs.cso");
	_psByteCode = D3DUtil::LoadBinary(L"Shaders\\color_ps.cso");

	_inputLayout = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
}

void BoxApp::BuildBoxGeometry() {
	std::array<Vertex, 8> vertices = {
		Vertex({XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White)}),
		Vertex({XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT4(Colors::Black)}),
		Vertex({XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT4(Colors::Red)}),
		Vertex({XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green)}),
		Vertex({XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT4(Colors::Blue)}),
		Vertex({XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT4(Colors::Yellow)}),
		Vertex({XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT4(Colors::Cyan)}),
		Vertex({XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT4(Colors::Magenta)})
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

	const size_t vbByteSize = vertices.size() * sizeof(Vertex);
	const size_t ibByteSize = indices.size() * sizeof(std::uint16_t);

	_boxGeo = std::make_unique<MeshGeometry>();
	_boxGeo->Name = "Box";

	//ThrowIfFailed (D3DCreateBlob (vbByteSize, &m_BoxGeo->VertexBufferCPU));
	//CopyMemory (m_BoxGeo->VertexBufferCPU->GetBufferPointer (), vertices.data (), vbByteSize);

	//ThrowIfFailed (D3DCreateBlob (ibByteSize, &m_BoxGeo->IndexBufferCPU));
	//CopyMemory (m_BoxGeo->IndexBufferCPU->GetBufferPointer (), indices.data (), ibByteSize);

	_boxGeo->VertexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(),
		_cmdList.Get(), vertices.data(), vbByteSize, _boxGeo->VertexBufferUploader);

	_boxGeo->IndexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(),
		_cmdList.Get(), indices.data(), ibByteSize, _boxGeo->IndexBufferUploader);

	_boxGeo->VertexByteStride = sizeof(Vertex);
	_boxGeo->VertexBufferByteSize = vbByteSize;
	_boxGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
	_boxGeo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	_boxGeo->DrawArgs["Box"] = submesh;
}

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
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = _backBufferFormat;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.DSVFormat = _depthStencilFormat;
	//psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&_pso)));

	// MSAA Pso
	D3D12_GRAPHICS_PIPELINE_STATE_DESC msaaPsoDesc = psoDesc;
	msaaPsoDesc.SampleDesc.Count = 4;
	msaaPsoDesc.SampleDesc.Quality = _4xMsaaQuality - 1;
	ThrowIfFailed(_device->CreateGraphicsPipelineState(&msaaPsoDesc, IID_PPV_ARGS(&_msaaPso)));
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

	XMMATRIX world = XMLoadFloat4x4(&_world);
	XMMATRIX proj = XMLoadFloat4x4(&_proj);
	XMMATRIX worldViewProj = world * view * proj;

	ObjectConstants objConstants;
	XMStoreFloat4x4(&objConstants.WorldViewProj, worldViewProj);
	_objectCB->CopyData(0, objConstants);
}

void BoxApp::Draw(const GameTimer& gt) {
	ThrowIfFailed(_cmdAllocator->Reset());

	ThrowIfFailed(_cmdList->Reset(_cmdAllocator.Get(), nullptr));

	_cmdList->RSSetViewports(1, &_viewport);
	_cmdList->RSSetScissorRects(1, &_scissorRect);

	if (_4xMsaaState) {
		_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_msaaRenderTargetBuffer.Get(),
								  D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET
		));

		auto rtvDescriptor = _msaaRtvHeap->GetCPUDescriptorHandleForHeapStart();
		auto dsvDescriptor = _msaaDsvHeap->GetCPUDescriptorHandleForHeapStart();
		_cmdList->ClearRenderTargetView(rtvDescriptor, Colors::LightSteelBlue, 0, nullptr);
		_cmdList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		_cmdList->OMSetRenderTargets(1, &rtvDescriptor, false, &dsvDescriptor);

		_cmdList->SetPipelineState(_msaaPso.Get());

		ID3D12DescriptorHeap* descriptorHeaps[] = {_cbvHeap.Get()};
		_cmdList->SetGraphicsRootSignature(_rootSignature.Get());
		_cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
		_cmdList->SetGraphicsRootDescriptorTable(0, _cbvHeap->GetGPUDescriptorHandleForHeapStart());

		_cmdList->IASetVertexBuffers(0, 1, &_boxGeo->VertexBufferView());
		_cmdList->IASetIndexBuffer(&_boxGeo->IndexBufferView());
		_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		_cmdList->DrawIndexedInstanced(_boxGeo->DrawArgs["Box"].IndexCount, 1, 0, 0, 0);

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
		_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
								  D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET
		));

		_cmdList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
		_cmdList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		_cmdList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

		_cmdList->SetPipelineState(_pso.Get());

		ID3D12DescriptorHeap* descriptorHeaps[] = {_cbvHeap.Get()};
		_cmdList->SetGraphicsRootSignature(_rootSignature.Get());
		_cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
		_cmdList->SetGraphicsRootDescriptorTable(0, _cbvHeap->GetGPUDescriptorHandleForHeapStart());

		_cmdList->IASetVertexBuffers(0, 1, &_boxGeo->VertexBufferView());
		_cmdList->IASetIndexBuffer(&_boxGeo->IndexBufferView());
		_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		_cmdList->DrawIndexedInstanced(_boxGeo->DrawArgs["Box"].IndexCount, 1, 0, 0, 0);

		_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
								  D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT
		));
	}

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
		_phi = MathHelper::Clamp(_phi, 0.1f, MathHelper::PI - 0.01f);
	} else if ((btnState & MK_RBUTTON) != 0) {
		float dx = 0.005f * static_cast<float> (x - _lastMousePos.x);
		float dy = 0.005f * static_cast<float> (y - _lastMousePos.y);

		_radius += dx - dy;

		_radius = MathHelper::Clamp(_radius, 3.0f, 15.0f);
	}

	_lastMousePos.x = x;
	_lastMousePos.y = y;
}
