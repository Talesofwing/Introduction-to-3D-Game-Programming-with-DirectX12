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
	XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4 ();
};

class BoxApp : public D3DApp {
public:

	BoxApp (HINSTANCE hInstance);
	BoxApp (const BoxApp& rhs) = delete;
	BoxApp& operator=(const BoxApp& rhs) = delete;
	~BoxApp ();

	virtual bool Initialize () override;

private:
	
	virtual void OnResize () override;
	virtual void Update (const GameTimer& gt) override;
	virtual void Draw (const GameTimer& gt) override;

	virtual void OnMouseDown (WPARAM btnState, int x, int y) override;
	virtual void OnMouseUp (WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove (WPARAM btnState, int x, int y) override;

	void BuildDescriptorHeaps ();
	void BuildConstantBuffers ();
	void BuildRootSignature ();
	void BuildShadersAndInputLayout ();
	void BuildBoxGeometry ();
	void BuildPSO ();

private:

	std::vector<D3D12_INPUT_ELEMENT_DESC> m_InputLayout;
	ComPtr<ID3D12PipelineState> m_PSO = nullptr;
	ComPtr<ID3D12RootSignature> m_RootSignature = nullptr;

	ComPtr<ID3D12DescriptorHeap> m_CBVHeap = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> m_ObjectCB = nullptr;

	std::unique_ptr<MeshGeometry> m_BoxGeo = nullptr;

	ComPtr<ID3DBlob> m_VSByteCode = nullptr;
	ComPtr<ID3DBlob> m_PSByteCode = nullptr;

	XMFLOAT4X4 m_World = MathHelper::Identity4x4 ();
	XMFLOAT4X4 m_View = MathHelper::Identity4x4 ();
	XMFLOAT4X4 m_Proj = MathHelper::Identity4x4 ();

	float m_Theta = 1.5f * XM_PI;
	float m_Phi = XM_PIDIV4;
	float m_Radius = 10.0f;			

	POINT m_LastMousePos;
};

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE prevInstance,
					PSTR cmdLine, int showCmd) {
	 // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag (_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try {
		BoxApp theApp (hInstance);
		if (!theApp.Initialize ())
			return 0;

		return theApp.Run ();
	} catch (DxException & e) {
		MessageBox (nullptr, e.ToString ().c_str (), L"HR Failed", MB_OK);
		return 0;
	}
}

BoxApp::BoxApp (HINSTANCE hInstance) : D3DApp (hInstance) {
	m_MainWndCaption = L"Chapter 6 - Box";
}

BoxApp::~BoxApp () {}

bool BoxApp::Initialize () {
	if (!D3DApp::Initialize ())
		return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed (m_CmdList->Reset (m_CmdAllocator.Get (), nullptr));

	BuildDescriptorHeaps ();
	BuildConstantBuffers ();
	BuildRootSignature ();
	BuildShadersAndInputLayout ();
	BuildBoxGeometry ();
	BuildPSO ();

	ThrowIfFailed (m_CmdList->Close ());
	ID3D12CommandList* cmdLists[] = {m_CmdList.Get ()};
	m_CmdQueue->ExecuteCommandLists (_countof (cmdLists), cmdLists);

	FlushCommandQueue ();

	m_BoxGeo->DisposeUploaders ();

	return true;
}

void BoxApp::BuildDescriptorHeaps () {
	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NumDescriptors = 1;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;
	ThrowIfFailed (m_Device->CreateDescriptorHeap (&cbvHeapDesc, IID_PPV_ARGS (&m_CBVHeap)));
}

void BoxApp::BuildConstantBuffers () {
	m_ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>> (m_Device.Get (), 1, true);

	UINT objCBByteSize = D3DUtil::CalcConstantBufferByteSize (sizeof (ObjectConstants));

	// 緩存區的起始地址
	D3D12_GPU_VIRTUAL_ADDRESS cbAddress = m_ObjectCB->Resource ()->GetGPUVirtualAddress ();
	
	// 偏移到常量綠沖區中繪制第i個物體所需的常量數據
	int boxCBufIndex = 0;
	cbAddress += boxCBufIndex * objCBByteSize;

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
	cbvDesc.BufferLocation = cbAddress;
	cbvDesc.SizeInBytes = objCBByteSize;

	m_Device->CreateConstantBufferView (&cbvDesc, m_CBVHeap->GetCPUDescriptorHandleForHeapStart ());
}

void BoxApp::BuildRootSignature () {
	// 根參數可以是描述符表、根描述符或根常量
	CD3DX12_ROOT_PARAMETER slotRootParameter[1];
	// 創建一個只存有一個CBV的描述符表
	CD3DX12_DESCRIPTOR_RANGE cbvTable;
	cbvTable.Init (
		D3D12_DESCRIPTOR_RANGE_TYPE_CBV,		// Type
		1,										// 表中的描述符數量
		0										// 將這段描述符區域綁定至此基準著色器寄存器
	);

	slotRootParameter[0].InitAsDescriptorTable (
		1,				// 描述符區域的數量
		&cbvTable		// 指向描述符區域數組的指針			
	);

	// 根簽名由一組根參數構成
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc (1, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// 創建僅含一個槽位(該槽位指向一個僅由單個常量緩沖區組成的描述符區域的根簽名
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	auto hr = D3D12SerializeRootSignature (
		&rootSigDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,	
		serializedRootSig.GetAddressOf (),
		errorBlob.GetAddressOf ()
	);

	if (errorBlob != nullptr) {
		OutputDebugStringA ((char*)errorBlob->GetBufferPointer ());
	}
	ThrowIfFailed (hr);

	ThrowIfFailed (m_Device->CreateRootSignature (
		0,
		serializedRootSig->GetBufferPointer (),
		serializedRootSig->GetBufferSize (),
		IID_PPV_ARGS (&m_RootSignature)
	));
}

void BoxApp::BuildShadersAndInputLayout () {
	m_VSByteCode = D3DUtil::CompileShader (L"Shaders\\Color.hlsl", nullptr, "VS", "vs_5_0");
	m_PSByteCode = D3DUtil::CompileShader (L"Shaders\\Color.hlsl", nullptr, "PS", "ps_5_0");

	m_InputLayout = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
}

void BoxApp::BuildBoxGeometry () {
	std::array<Vertex, 8> vertices = {
		Vertex ({XMFLOAT3 (-1.0f, -1.0f, -1.0f), XMFLOAT4 (Colors::White)}),
		Vertex ({XMFLOAT3 (-1.0f,  1.0f, -1.0f), XMFLOAT4 (Colors::Black)}),
		Vertex ({XMFLOAT3 (1.0f,  1.0f, -1.0f), XMFLOAT4 (Colors::Red)}),
		Vertex ({XMFLOAT3 (1.0f, -1.0f, -1.0f), XMFLOAT4 (Colors::Green)}),
		Vertex ({XMFLOAT3 (-1.0f, -1.0f,  1.0f), XMFLOAT4 (Colors::Blue)}),
		Vertex ({XMFLOAT3 (-1.0f,  1.0f,  1.0f), XMFLOAT4 (Colors::Yellow)}),
		Vertex ({XMFLOAT3 (1.0f,  1.0f,  1.0f), XMFLOAT4 (Colors::Cyan)}),
		Vertex ({XMFLOAT3 (1.0f, -1.0f,  1.0f), XMFLOAT4 (Colors::Magenta)})
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

	const UINT vbByteSize = vertices.size () * sizeof (Vertex);
	const UINT ibByteSize = indices.size () * sizeof (std::uint16_t);

	m_BoxGeo = std::make_unique<MeshGeometry> ();
	m_BoxGeo->Name = "Box";
	
	ThrowIfFailed (D3DCreateBlob (vbByteSize, &m_BoxGeo->VertexBufferCPU));
	CopyMemory (m_BoxGeo->VertexBufferCPU->GetBufferPointer (), vertices.data (), vbByteSize);

	ThrowIfFailed (D3DCreateBlob (ibByteSize, &m_BoxGeo->IndexBufferCPU));
	CopyMemory (m_BoxGeo->IndexBufferCPU->GetBufferPointer (), indices.data (), ibByteSize);

	m_BoxGeo->VertexBufferGPU = D3DUtil::CreateDefaultBuffer (m_Device.Get (),
															  m_CmdList.Get (), vertices.data (), vbByteSize, m_BoxGeo->VertexBufferUploader);

	m_BoxGeo->IndexBufferGPU = D3DUtil::CreateDefaultBuffer (m_Device.Get (),
															  m_CmdList.Get (), indices.data (), ibByteSize, m_BoxGeo->IndexBufferUploader);

	m_BoxGeo->VertexByteStride = sizeof (Vertex);
	m_BoxGeo->VertexBufferByteSize = vbByteSize;
	m_BoxGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
	m_BoxGeo->IndexBufferByteSize = ibByteSize;

	SubMeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size ();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	m_BoxGeo->DrawArgs["Box"] = submesh;
}

void BoxApp::BuildPSO () {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ZeroMemory (&psoDesc, sizeof (D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.InputLayout = {m_InputLayout.data (), (UINT)m_InputLayout.size ()};
	psoDesc.pRootSignature = m_RootSignature.Get ();
	psoDesc.VS = {
		m_VSByteCode->GetBufferPointer (),
		m_VSByteCode->GetBufferSize ()
	};
	psoDesc.PS = {
		m_PSByteCode->GetBufferPointer (),
		m_PSByteCode->GetBufferSize ()
	};
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC (D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC (D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC (D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = m_BackBufferFormat;
	psoDesc.SampleDesc.Count = m_4xMsaaState ? 4 : 1;
	psoDesc.SampleDesc.Quality = m_4xMsaaState ? (m_4xMsaaQuality - 1) : 0;
	psoDesc.DSVFormat = m_DepthStencilFormat;
	ThrowIfFailed (m_Device->CreateGraphicsPipelineState (&psoDesc, IID_PPV_ARGS (&m_PSO)));
}

void BoxApp::OnResize () {
	D3DApp::OnResize ();

	// 若用戶調整了窗口尺寸, 則更新緃棋比並重新計算投影矩陣
	XMMATRIX P = XMMatrixPerspectiveFovLH (0.25f * MathHelper::PI, AspectRatio (), 1.0f, 1000.0f);
	XMStoreFloat4x4 (&m_Proj, P);
}

void BoxApp::Update (const GameTimer& gt) {
	// 由球坐標轉換為笛卡兒坐標
	float x = m_Radius * sinf (m_Phi) * cosf (m_Theta);
	float z = m_Radius * sinf (m_Phi) * sinf (m_Theta);
	float y = m_Radius * cosf (m_Phi);

	// 構建觀察矩陣
	XMVECTOR pos = XMVectorSet (x, y, z, 1.0f);
	XMVECTOR target = XMVectorZero ();
	XMVECTOR up = XMVectorSet (0.0f, 1.0f, 0.0f, 0.0f);
	XMMATRIX view = XMMatrixLookAtLH (pos, target, up);
	XMStoreFloat4x4 (&m_View, view);

	XMMATRIX world = XMLoadFloat4x4 (&m_World);
	XMMATRIX proj = XMLoadFloat4x4 (&m_Proj);
	XMMATRIX worldViewProj = world * view * proj;

	// 用最新的worldViewProj矩陣來更新常量緩沖區
	ObjectConstants objConstants;
	XMStoreFloat4x4 (&objConstants.WorldViewProj, worldViewProj);
	m_ObjectCB->CopyData (0, objConstants);
}

void BoxApp::Draw (const GameTimer& gt) {
	// 复用記錄命令所用的內存
	// 只有當GPU中的命令列表執行完畢後, 才可以對其進行重置
	ThrowIfFailed (m_CmdAllocator->Reset ());

	// 通過函數ExecuteCommandList將命令列表加入命令隊列後,便可對它進行重置
	// 复用命令列表即复用其相應的內存
	ThrowIfFailed (m_CmdList->Reset (m_CmdAllocator.Get (), m_PSO.Get ()));

	m_CmdList->RSSetViewports (1, &m_Viewport);
	m_CmdList->RSSetScissorRects (1, &m_ScissorRect);

	// 按照資源的用途指示其狀態的轉變, 此處將資源從呈現狀態轉換為渲染目標狀態
	m_CmdList->ResourceBarrier (1, &CD3DX12_RESOURCE_BARRIER::Transition (CurrentBackBuffer (),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET
	));

	// 清除後台緩沖區和深度緩沖區
	m_CmdList->ClearRenderTargetView (CurrentBackBufferView (), Colors::LightSteelBlue, 0, nullptr);
	m_CmdList->ClearDepthStencilView (DepthStencilView (), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// 指定將要渲染的目標緩沖區
	m_CmdList->OMSetRenderTargets (1, &CurrentBackBufferView (), true, &DepthStencilView ());

	ID3D12DescriptorHeap* descriptorHeaps[] = {m_CBVHeap.Get ()};
	m_CmdList->SetDescriptorHeaps (_countof (descriptorHeaps), descriptorHeaps);

	m_CmdList->SetGraphicsRootSignature (m_RootSignature.Get ());
	
	m_CmdList->IASetVertexBuffers (0, 1, &m_BoxGeo->VertexBufferView ());
	m_CmdList->IASetIndexBuffer (&m_BoxGeo->IndexBufferView ());
	m_CmdList->IASetPrimitiveTopology (D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_CmdList->SetGraphicsRootDescriptorTable (0, m_CBVHeap->GetGPUDescriptorHandleForHeapStart ());

	m_CmdList->DrawIndexedInstanced (m_BoxGeo->DrawArgs["Box"].IndexCount, 1, 0, 0, 0);

	// 按照資源的用途指示其狀態的轉換, 此處將資源從渲染目標狀態轉換為呈現狀態
	m_CmdList->ResourceBarrier (1, &CD3DX12_RESOURCE_BARRIER::Transition (CurrentBackBuffer (),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT
	));

	// 完成命令的記錄
	ThrowIfFailed (m_CmdList->Close ());
	
	// 向命令隊列添加欲執行的命令列表
	ID3D12CommandList* cmdLists[] = {m_CmdList.Get ()};
	m_CmdQueue->ExecuteCommandLists (_countof (cmdLists), cmdLists);

	// 交換後台緩沖區與前台緩沖區
	ThrowIfFailed (m_SwapChain->Present (0, 0));
	m_CurrentBackBuffer = (m_CurrentBackBuffer + 1) % m_SwapChainBufferCount;

	// 等待繪制此幀的一系列命令執行完畢。這種等待的方法雖然簡單但也低效
	FlushCommandQueue ();
}

void BoxApp::OnMouseDown (WPARAM btnState, int x, int y) {
	m_LastMousePos.x = x;
	m_LastMousePos.y = y;

	SetCapture (m_MainWnd);
}

void BoxApp::OnMouseUp (WPARAM btnState, int x, int y) {
	ReleaseCapture ();
}

void BoxApp::OnMouseMove (WPARAM btnState, int x, int y) {
	if ((btnState & MK_LBUTTON) != 0) {
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians (0.25f * static_cast<float> (x - m_LastMousePos.x));
		float dy = XMConvertToRadians (0.25f * static_cast<float> (y - m_LastMousePos.y));

		// Update angles based on input to orbit camera around box.
		m_Theta += dx;
		m_Phi += dy;

		// Restrict the angle m_Phi.
		// 0 and 180 not included.
		m_Phi = MathHelper::Clamp (m_Phi, 0.1f, MathHelper::PI - 0.1f);
	} else if ((btnState & MK_RBUTTON) != 0) {
		// 使場景中的每個像素按鼠標移動距離的0.005倍進行縮放
		float dx = 0.005f * static_cast<float> (x - m_LastMousePos.x);
		float dy = 0.005f * static_cast<float> (y - m_LastMousePos.y);

		// 根據鼠標的輸入更新攝像機可視範圍半徑
		m_Radius += dx - dy;

		// 限制可視半徑的範圍
		m_Radius = MathHelper::Clamp (m_Radius, 3.0f, 15.0f);
	}

	m_LastMousePos.x = x;
	m_LastMousePos.y = y;
}