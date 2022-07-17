#include "../../../Common//D3DApp.h"

using namespace DirectX;

struct Vertex {
	XMFLOAT3 Pos;
};

class RenderingPipelineApp : public D3DApp {

public:

	RenderingPipelineApp (HINSTANCE hInstance);
	~RenderingPipelineApp ();

	virtual bool Initialize () override;

private:

	virtual void Update (const GameTimer& gt) override;
	virtual void Draw (const GameTimer& gt) override;

	void BuildPyramidGeometry ();
	void BuildCombinationVertexAndIndex ();

};

RenderingPipelineApp::RenderingPipelineApp (HINSTANCE hInstance) : D3DApp (hInstance) {
	m_MainWndCaption = L"Chapter - 5, Exercise";
}

RenderingPipelineApp::~RenderingPipelineApp () {}

bool RenderingPipelineApp::Initialize () {
	if (!D3DApp::Initialize ())
		return false;

	BuildPyramidGeometry ();

	return true;
}

void RenderingPipelineApp::BuildPyramidGeometry () {
	std::array<Vertex, 8> vertices = {
		Vertex ({XMFLOAT3 ( 0.0f,  2.0f,  0.0f)}),	// top
		Vertex ({XMFLOAT3 (-1.0f,  0.0f, -1.0f)}),
		Vertex ({XMFLOAT3 (-1.0f,  0.0f,  1.0f)}),
		Vertex ({XMFLOAT3 ( 1.0f,  0.0f,  1.0f)}),
		Vertex ({XMFLOAT3 ( 1.0f,  0.0f, -1.0f)})
	};

	std::array<std::uint16_t, 18> indices = {
		1, 0, 4,
		2, 0, 1,
		3, 0, 2,
		4, 0, 3,
		1, 2, 4,
		2, 3, 4
	};
}

void RenderingPipelineApp::BuildCombinationVertexAndIndex () {
	std::array<Vertex, 13> vertices = {
		// parallelogram
		Vertex ({XMFLOAT3 (-1.0f, -1.0f, 0.0f)}),
		Vertex ({XMFLOAT3 (-0.5f,  1.0f, 0.0f)}),
		Vertex ({XMFLOAT3 ( 1.0f,  1.0f, 0.0f)}),
		Vertex ({XMFLOAT3 ( 0.5f, -1.0f, 0.0f)}),
		// polygon
		Vertex ({XMFLOAT3 ( 0.0f,  0.0f, 0.0f)}),	// center
		Vertex ({XMFLOAT3 ( 2.0f,  0.0f, 0.0f)}),	// right
		Vertex ({XMFLOAT3 ( 1.0f, -0.5f, 0.0f)}),
		Vertex ({XMFLOAT3 ( 0.0f, -1.0f, 0.0f)}),	// bottom
		Vertex ({XMFLOAT3 (-1.0f, -0.5f, 0.0f)}),
		Vertex ({XMFLOAT3 (-2.0f,  0.0f, 0.0f)}),	// left
		Vertex ({XMFLOAT3 (-1.0f,  0.5f, 0.0f)}),
		Vertex ({XMFLOAT3 ( 0.0f,  1.0f, 0.0f)}),	// top
		Vertex ({XMFLOAT3 ( 1.0f,  0.5f, 0.0f)}),
	};

	std::array<std::uint16_t, 27> indices = {
		// parallelogram
		0, 1, 2,
		0, 2, 3,
		// polygon
		// all + 4 (parallelogram has four vertex)
		// so polygon is 4 first
		0 + 4, 1 + 4, 2 + 4,
		0 + 4, 2 + 4, 3 + 4,
		0 + 4, 3 + 4, 4 + 4,
		0 + 4, 4 + 4, 5 + 4,
		0 + 4, 5 + 4, 6 + 4,
		0 + 4, 6 + 4, 7 + 4,
		0 + 4, 7 + 4, 8 + 4
	};
}

void RenderingPipelineApp::Update (const GameTimer& gt) {}

void RenderingPipelineApp::Draw (const GameTimer& gt) {
	ThrowIfFailed (m_CmdAllocator->Reset ());

	ThrowIfFailed (m_CmdList->Reset (m_CmdAllocator.Get (), nullptr));

	m_CmdList->ResourceBarrier (1, &CD3DX12_RESOURCE_BARRIER::Transition (CurrentBackBuffer (),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	m_CmdList->RSSetViewports (1, &m_Viewport);
	m_CmdList->RSSetScissorRects (1, &m_ScissorRect);

	m_CmdList->ClearRenderTargetView (CurrentBackBufferView (), Colors::LightSteelBlue, 0, nullptr);
	m_CmdList->ClearDepthStencilView (DepthStencilView (), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	m_CmdList->OMSetRenderTargets (1, &CurrentBackBufferView (), true, &DepthStencilView ());

	m_CmdList->ResourceBarrier (1, &CD3DX12_RESOURCE_BARRIER::Transition (CurrentBackBuffer (),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed (m_CmdList->Close ());

	ID3D12CommandList* cmdsLists[] = { m_CmdList.Get () };
	m_CmdQueue->ExecuteCommandLists (_countof (cmdsLists), cmdsLists);

	ThrowIfFailed (m_SwapChain->Present (0, 0));

	m_CurrentBackBuffer = (m_CurrentBackBuffer + 1) % m_SwapChainBufferCount;

	FlushCommandQueue ();
}

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int showCmd) {
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag (_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try {
		RenderingPipelineApp theApp (hInstance);

		if (!theApp.Initialize ())
			return 0;

		return theApp.Run ();
	}
	catch (DxException& e) {
		MessageBox (nullptr, e.ToString ().c_str (), L"HR Failed", MB_OK);
		return 0;
	}
}