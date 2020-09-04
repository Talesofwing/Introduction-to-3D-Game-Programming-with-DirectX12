#include "D3DApp.h"

#include <windowsx.h>

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;

LRESULT CALLBACK MainWndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	return D3DApp::GetApp ()->MsgProc (hwnd, msg, wParam, lParam);
}

LRESULT D3DApp::MsgProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		// WM_ACTIVATE is sent when the window is activated or deactivated.
		// We pause the game when the window is deactivated and unpause it
		// when it becomes active.
		case WM_ACTIVATE:
			if (LOWORD (wParam) == WA_INACTIVE) {
				m_AppPaused = true;
				m_Timer.Stop ();
			} else {
				m_AppPaused = false;
				m_Timer.Start ();
			}
			break;
		// WM_SIZE is sent when the user resizes the window.
		case WM_SIZE:
			// Save the new client area dimensions.
			m_ClientWidth = LOWORD (lParam);
			m_ClientHeight = HIWORD (lParam);
			if (m_Device) {
				if (wParam == SIZE_MINIMIZED) {
					m_AppPaused = true;
					m_Minimized = true;
					m_Maximized = false;
				} else if (wParam == SIZE_MAXIMIZED) {
					m_AppPaused = false;
					m_Minimized = false;
					m_Maximized = true;
					OnResize ();
				} else if (wParam == SIZE_RESTORED) {
					// Restoring from minimized state?
					if (m_Minimized) {
						m_AppPaused = false;
						m_Minimized = false;
						OnResize ();
					// Restoring from maximized state?
					} else if (m_Maximized) {
						m_AppPaused = false;
						m_Maximized = false;
						OnResize ();
					} else if (m_Resizing) {
						// If user is dragging the resize bars, we do not resize 
						// the buffers here because as the user continuously 
						// drags the resize bars, a stream of WM_SIZE messages are
						// sent to the window, and it would be pointless (and slow)
						// to resize for each WM_SIZE message received from dragging
						// the resize bars.  So instead, we reset after the user is 
						// done resizing the window and releases the resize bars, which 
						// sends a WM_EXITSIZEMOVE message.
					} else {		// API call such as SetWindowPos or m_SwapChain->SetFullscreenState.
						OnResize ();
					}
				}
			}
			break;
		// WM_ENTERSIZEMOVE is sent when the user grabs the resize bars.
		case WM_ENTERSIZEMOVE:
			m_AppPaused = true;
			m_Resizing = true;
			m_Timer.Stop ();
			break;
		// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
		// Here we reset everything based on the new window dimensions.
		case WM_EXITSIZEMOVE:
			m_AppPaused = false;
			m_Resizing = false;
			m_Timer.Start ();
			OnResize ();
			break;
		// WM_DESTROY is sent when the window is being destroyed.
		case WM_DESTROY:
			PostQuitMessage (0);
			break;
		// The WM_MENUCHAR message is sent when a menu is active and the user presses
		// a key that does not correspond to any mnemonic or accelerator key.
		case WM_MENUCHAR:
			// Don't beep when we alt-enter
			return MAKELRESULT (0, MNC_CLOSE);
		// Catch this message so to prevent the window from becoming too small.
		case WM_GETMINMAXINFO:
			// 控制窗口大小的範圍以及最大化時出現的位置
			// 在移動窗口或改變窗口大小時觸發
			((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
			((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
			break;
		case WM_LBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_RBUTTONDOWN:
			OnMouseDown (wParam, GET_X_LPARAM (lParam), GET_Y_LPARAM (lParam));
			break;
		case WM_LBUTTONUP:
		case WM_MBUTTONUP:
		case WM_RBUTTONUP:
			OnMouseUp (wParam, GET_X_LPARAM (lParam), GET_Y_LPARAM (lParam));
			break;
		case WM_MOUSEMOVE:
			OnMouseMove (wParam, GET_X_LPARAM (lParam), GET_Y_LPARAM (lParam));
			break;
		case WM_KEYUP:
			if (wParam == VK_ESCAPE) {
				PostQuitMessage (0);
			} else if ((int)wParam == VK_F2) {
				Set4xMsaaState (!m_4xMsaaState);
			}
			break;
	}

	return DefWindowProc (hwnd, msg, wParam, lParam);
}

D3DApp* D3DApp::m_Instance = nullptr;
D3DApp* D3DApp::GetApp () {
	return m_Instance;
}

D3DApp::D3DApp (HINSTANCE hInstance) : m_AppInstance (hInstance) {
	assert (m_Instance == nullptr);
	m_Instance = this;
}

D3DApp::~D3DApp () {
	if (m_Device != nullptr)
		FlushCommandQueue ();
}

bool D3DApp::Initialize () {
	if (!InitMainWindow ())
		return false;

	if (!InitDirect3D ())
		return false;

	// Do the initial resize code.
	OnResize ();

	return true;
}

bool D3DApp::InitMainWindow () {
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = m_AppInstance;
	wc.hIcon = LoadIcon (0, IDI_APPLICATION);
	wc.hCursor = LoadCursor (0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject (NULL_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = L"MainWnd";

	if (!RegisterClass (&wc)) {
		MessageBox (0, L"RegisterClass Failed.", 0, 0);
		return false;
	}

	// Compute window rectangle dimensions based on requested client area dimensions.
	RECT R = {0, 0, m_ClientWidth, m_ClientHeight};
	AdjustWindowRect (&R, WS_OVERLAPPEDWINDOW, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;

	m_MainWnd = CreateWindow (L"MainWnd", m_MainWndCaption.c_str (),
							 WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, m_AppInstance, 0);

	if (!m_MainWnd) {
		MessageBox (0, L"CreateWindow Failed.", 0, 0);
		return false;
	}

	ShowWindow (m_MainWnd, SW_SHOW);
	UpdateWindow (m_MainWnd);

	return true;
}

bool D3DApp::InitDirect3D () {

#if defined(DEBUG) || defined (_DEBUG)
	// Enable the D3D12 debug layer.
	{
		ComPtr<ID3D12Debug> debugController;
		ThrowIfFailed (D3D12GetDebugInterface (IID_PPV_ARGS (&debugController)));
		debugController->EnableDebugLayer ();
	}
#endif
	
	ThrowIfFailed (CreateDXGIFactory1 (IID_PPV_ARGS (&m_DxgiFactory)));

	//
	// Try to create hardware device.
	//

	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_3,
		D3D_FEATURE_LEVEL_9_2,
		D3D_FEATURE_LEVEL_9_1,
	};

	HRESULT hardwareResult;
	for (auto level : levels) {
		hardwareResult = D3D12CreateDevice (
			nullptr,		// default
			level,
			IID_PPV_ARGS (&m_Device)
		);
		if (hardwareResult == S_OK) {
			break;
		}
	}

	// Fallback to WARP device.
	if (FAILED (hardwareResult)) {
		ComPtr<IDXGIAdapter> pWarpAdapter;
		ThrowIfFailed (m_DxgiFactory->EnumWarpAdapter (IID_PPV_ARGS (&pWarpAdapter)));

		for (auto level : levels) {
			ThrowIfFailed (D3D12CreateDevice (pWarpAdapter.Get (), level, IID_PPV_ARGS (&m_Device)));
		}
	}

	//
	// Create Fence
	//

	ThrowIfFailed (m_Device->CreateFence (0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS (&m_Fence)));

	// Save the size of view.
	m_RtvDescriptorSize = m_Device->GetDescriptorHandleIncrementSize (D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_DsvDescriptorSize = m_Device->GetDescriptorHandleIncrementSize (D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	m_CbvSrvUavDescriptorSize = m_Device->GetDescriptorHandleIncrementSize (D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// Chcek 4X MSAA quality support for our back buffer format.
	// All Direct3D 11 capable devices support 4X MSAA for all render
	// target formats, so we only need to check quality support.
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
	msQualityLevels.Format = m_BackBufferFormat;
	msQualityLevels.SampleCount = 4;
	msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	msQualityLevels.NumQualityLevels = 0;
	ThrowIfFailed (m_Device->CheckFeatureSupport (
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&msQualityLevels,
		sizeof (msQualityLevels)
	));

	m_4xMsaaQuality = msQualityLevels.NumQualityLevels;
	assert (m_4xMsaaQuality > 0 && "Unexpected MSAA quality level.");

#ifdef _DEBUG
	LogAdapters ();
#endif

	CreateCommandObjects ();
	CreateSwapChain ();
	CreateRtvAndDsvDescriptorHeaps ();

	return true;
}

int D3DApp::Run () {
	MSG msg = {0};

	m_Timer.Reset ();

	while (msg.message != WM_QUIT) {
		// If there are Window messages then process them.
		if (PeekMessage (&msg, 0, 0, 0, PM_REMOVE)) {
			TranslateMessage (&msg);
			DispatchMessage (&msg);
		}
		// Otherwise, do animation/game stuff.
		else {
			m_Timer.Tick ();

			if (!m_AppPaused) {
				CalculateFrameStats ();
				Update (m_Timer);
				Draw (m_Timer);
			} else {
				Sleep (100);
			}
		}
	}

	return (int)msg.wParam;
}

void D3DApp::CreateCommandObjects () {
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed (m_Device->CreateCommandQueue (&queueDesc, IID_PPV_ARGS (&m_CmdQueue)));

	ThrowIfFailed (m_Device->CreateCommandAllocator (
					D3D12_COMMAND_LIST_TYPE_DIRECT, 
					IID_PPV_ARGS (m_CmdAllocator.GetAddressOf ())
				  )
	);

	ThrowIfFailed (m_Device->CreateCommandList (
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			m_CmdAllocator.Get (),		// Associated command allocator
			nullptr,					// Initial PipelineStateObject
			IID_PPV_ARGS (m_CmdList.GetAddressOf ())
		)
	);

	// Start off in a closed state. This is because the first time we refer
	// to the command list we will Reset it, and it needs to be closed before
	// calling Reset.
	m_CmdList->Close ();
}

void D3DApp::CreateSwapChain () {
	// Release the previous swapchain we will be recreating.
	m_SwapChain.Reset ();

	DXGI_SWAP_CHAIN_DESC sd = {};
	sd.BufferDesc.Width = m_ClientWidth;
	sd.BufferDesc.Height = m_ClientHeight;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = m_BackBufferFormat;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;		// 逐行掃描 vs 隔行掃瞄
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;						// 圖像如何相對於屏幕進行拉伸
	sd.SampleDesc.Count = m_4xMsaaState ? 4 : 1;
	sd.SampleDesc.Quality = m_4xMsaaState ? (m_4xMsaaQuality - 1) : 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;							
	sd.BufferCount = m_SwapChainBufferCount;
	sd.OutputWindow = m_MainWnd;
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// Note: Swap chain uses queue to perform flush.
	ThrowIfFailed (m_DxgiFactory->CreateSwapChain (
						m_CmdQueue.Get (),
						&sd,
						m_SwapChain.GetAddressOf ()
				  )
	);
}

void D3DApp::CreateRtvAndDsvDescriptorHeaps () {
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = m_SwapChainBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed (m_Device->CreateDescriptorHeap (
					&rtvHeapDesc, IID_PPV_ARGS (m_RTVHeap.GetAddressOf ())
				  )
	);

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed (m_Device->CreateDescriptorHeap (
					&dsvHeapDesc, IID_PPV_ARGS (m_DSVHeap.GetAddressOf ())
				  )
	);
}

void D3DApp::OnResize () {
	assert (m_Device);
	assert (m_SwapChain);
	assert (m_CmdAllocator);

	// Flush before changing any resources
	FlushCommandQueue ();

	ThrowIfFailed (m_CmdList->Reset (m_CmdAllocator.Get (), nullptr));

	// Release the previous resources we will be recreating.
	for (int i = 0; i < m_SwapChainBufferCount; ++i)
		m_SwapChainBuffer[i].Reset ();
	m_DepthStencilBuffer.Reset ();

	// Resize the swap chain.
	ThrowIfFailed (m_SwapChain->ResizeBuffers (
						m_SwapChainBufferCount,
						m_ClientWidth, m_ClientHeight,
						m_BackBufferFormat,
						DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
					)
	);

	m_CurrentBackBuffer = 0;

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle (m_RTVHeap->GetCPUDescriptorHandleForHeapStart ());
	for (UINT i = 0; i < m_SwapChainBufferCount; i++) {
		ThrowIfFailed (m_SwapChain->GetBuffer (i, IID_PPV_ARGS (&m_SwapChainBuffer [i])));
		m_Device->CreateRenderTargetView (m_SwapChainBuffer [i].Get (), nullptr, rtvHeapHandle);
		rtvHeapHandle.Offset (1, m_RtvDescriptorSize);
	}

	// Create the depth/stencil buffer and view.
	D3D12_RESOURCE_DESC depthStencilDesc;
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = m_ClientWidth;
	depthStencilDesc.Height = m_ClientHeight;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	depthStencilDesc.SampleDesc.Count = m_4xMsaaState ? 4 : 1;
	depthStencilDesc.SampleDesc.Quality = m_4xMsaaState ? (m_4xMsaaQuality - 1) : 0;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear;
	optClear.Format = m_DepthStencilFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;
	ThrowIfFailed (m_Device->CreateCommittedResource (
		&CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS (m_DepthStencilBuffer.GetAddressOf ())
	));

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = m_DepthStencilFormat;
	dsvDesc.Texture2D.MipSlice = 0;
	m_Device->CreateDepthStencilView (m_DepthStencilBuffer.Get (), &dsvDesc, DepthStencilView ());

	// Transition the resource from its initial state to be used as a depth buffer.
	m_CmdList->ResourceBarrier (1, &CD3DX12_RESOURCE_BARRIER::Transition (m_DepthStencilBuffer.Get (),
																		 D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	// Execute the resize commands.
	ThrowIfFailed (m_CmdList->Close ());
	ID3D12CommandList* cmdsLists[] = {m_CmdList.Get ()};
	m_CmdQueue->ExecuteCommandLists (_countof (cmdsLists), cmdsLists);

	// Wait until resize is complete.
	FlushCommandQueue ();

	// Update the viewport transform to cover the client area.
	m_Viewport.TopLeftX = 0;
	m_Viewport.TopLeftY = 0;
	m_Viewport.Width = static_cast<float> (m_ClientWidth);
	m_Viewport.Height = static_cast<float> (m_ClientHeight);
	m_Viewport.MinDepth = 0.0f;	
	m_Viewport.MaxDepth = 1.0f;

	m_ScissorRect = {0, 0, m_ClientWidth, m_ClientHeight};
}

void D3DApp::FlushCommandQueue () {
	m_CurrentFence++;

	ThrowIfFailed (m_CmdQueue->Signal (m_Fence.Get (), m_CurrentFence));

	if (m_Fence->GetCompletedValue () < m_CurrentFence) {
		HANDLE eventHandle = CreateEventEx (nullptr, false, false, EVENT_ALL_ACCESS);

		ThrowIfFailed (m_Fence->SetEventOnCompletion (m_CurrentFence, eventHandle));

		WaitForSingleObject (eventHandle, INFINITE);
		CloseHandle (eventHandle);
	}
}

ID3D12Resource* D3DApp::CurrentBackBuffer () const {
	return m_SwapChainBuffer[m_CurrentBackBuffer].Get ();
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::CurrentBackBufferView () const {
	/* ﾏ牋肥ｶ
		D3D12_CPU_DESCRIPTOR_HANDLE rtvH = m_RTVHeap->GetCPUDescriptorHandleForHeapStart ();
		rtvH.ptr += m_CurrentBackBuffer * m_RTVDescriptorSize;
		return rtvH;
	*/

	return CD3DX12_CPU_DESCRIPTOR_HANDLE (
		m_RTVHeap->GetCPUDescriptorHandleForHeapStart (),
		m_CurrentBackBuffer,
		m_RtvDescriptorSize
	);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::DepthStencilView () const {
	return m_DSVHeap->GetCPUDescriptorHandleForHeapStart ();
}

void D3DApp::CalculateFrameStats () {
	// Code computes the average frames per second, and also the
	// average time it takes to render one frame. These stats
	// are appended to the window caption bar.

	static int frameCnt = 0;
	static float timeElapsed = 0.0f;
	
	frameCnt++;

	if (m_Timer.TotalTime () - timeElapsed >= 1.0f) {
		float fps = (float)frameCnt;
		float mspf = 1000.0f / fps;

		wstring fpsStr = to_wstring (fps);
		wstring mspfStr = to_wstring (mspf);

		wstring windowText = m_MainWndCaption +
			L"    fps: " + fpsStr +
			L"   mspf: " + mspfStr;

		SetWindowText (m_MainWnd, windowText.c_str ());

		// Reset for next average.
		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}

HINSTANCE D3DApp::AppInstance() const {
	return m_AppInstance;
}

HWND D3DApp::MainWnd () const {
	return m_MainWnd;
}

float D3DApp::AspectRatio () const {
	return static_cast<float> (m_ClientWidth) / m_ClientHeight;
}

bool D3DApp::Get4xMsaaState () const {
	return m_4xMsaaState;
}

void D3DApp::Set4xMsaaState (bool value) {
	if (m_4xMsaaState != value) {
		m_4xMsaaState = value;
	
		// Recreate the swapchain and buffers with new multisample settings
		CreateSwapChain ();
		OnResize ();
	}
}

void D3DApp::LogAdapters () {
	UINT i = 0;
	IDXGIAdapter* adapter = nullptr;
	std::vector<IDXGIAdapter*> adapterList;
	while (m_DxgiFactory->EnumAdapters (i, &adapter) != DXGI_ERROR_NOT_FOUND) {
		DXGI_ADAPTER_DESC desc;
		adapter->GetDesc (&desc);

		std::wstring text = L"***Adapter: ";
		text += desc.Description;
		text += L"\n";

		OutputDebugString (text.c_str ());

		adapterList.push_back (adapter);

		++i;
	}

	for (size_t i = 0; i < adapterList.size (); ++i) {
		LogAdapterOutputs (adapterList[i]);
		ReleaseCom (adapterList[i]);
	}
}

void D3DApp::LogAdapterOutputs (IDXGIAdapter* adapter) {
	UINT i = 0;
	IDXGIOutput* output = nullptr;
	while (adapter->EnumOutputs (i, &output) != DXGI_ERROR_NOT_FOUND) {
		DXGI_OUTPUT_DESC desc;
		output->GetDesc (&desc);

		std::wstring text = L"***Output: ";
		text += desc.DeviceName;
		text += L"\n";
		OutputDebugString (text.c_str ());

		LogOutputDisplayModes (output, m_BackBufferFormat);

		ReleaseCom (output);

		++i;
	}
}

void D3DApp::LogOutputDisplayModes (IDXGIOutput* output, DXGI_FORMAT format) {
	UINT count = 0;
	UINT flags = 0;

	// Call with nullptr to get list count.
	output->GetDisplayModeList (format, flags, &count, nullptr);

	std::vector<DXGI_MODE_DESC> modeList (count);
	output->GetDisplayModeList (format, flags, &count, &modeList[0]);

	for (auto& x : modeList) {
		UINT n = x.RefreshRate.Numerator;
		UINT d = x.RefreshRate.Denominator;
		std::wstring text =
			L"Width = " + std::to_wstring (x.Width) + L" " +
			L"Height = " + std::to_wstring (x.Height) + L" " +
			L"Refresh = " + std::to_wstring (n) + L"/" + std::to_wstring (d) +
			L"\n";

		::OutputDebugString (text.c_str ());
	}
}