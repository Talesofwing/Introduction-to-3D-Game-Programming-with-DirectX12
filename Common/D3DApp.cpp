#include "D3DApp.h"

#include <windowsx.h>

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	return D3DApp::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}

LRESULT D3DApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_ACTIVATE:
			// WM_ACTIVATE is sent when the window is activated or deactivated.
			// We pause the game when the window is deactivated and unpause it
			// when it becomes active.
			if (LOWORD(wParam) == WA_INACTIVE) {
				_appPaused = true;
				_timer.Stop();
			} else {
				_appPaused = false;
				_timer.Start();
			}
			break;
		case WM_SIZE:
			// WM_SIZE is sent when the user resizes the window.

			// Save the new client area dimensions.
			_clientWidth = LOWORD(lParam);
			_clientHeight = HIWORD(lParam);
			if (_device) {
				if (wParam == SIZE_MINIMIZED) {
					_appPaused = true;
					_minimized = true;
					_maximized = false;
				} else if (wParam == SIZE_MAXIMIZED) {
					_appPaused = false;
					_minimized = false;
					_maximized = true;
					OnResize();
				} else if (wParam == SIZE_RESTORED) {
					if (_minimized) {
						// Restoring from minimized state?
						_appPaused = false;
						_minimized = false;
						OnResize();
					} else if (_maximized) {
						// Restoring from maximized state?
						_appPaused = false;
						_maximized = false;
						OnResize();
					} else if (_resizing) {
						// If user is dragging the resize bars, we do not resize 
						// the buffers here because as the user continuously 
						// drags the resize bars, a stream of WM_SIZE messages are
						// sent to the window, and it would be pointless (and slow)
						// to resize for each WM_SIZE message received from dragging
						// the resize bars.  So instead, we reset after the user is 
						// done resizing the window and releases the resize bars, which 
						// sends a WM_EXITSIZEMOVE message.
					} else {
						// API call such as SetWindowPos or SwapChain->SetFullscreenState.
						OnResize();
					}
				}
			}
			break;
		case WM_ENTERSIZEMOVE:
			// WM_ENTERSIZEMOVE is sent when the user grabs the resize bars.
			_appPaused = true;
			_resizing = true;
			_timer.Stop();
			break;
		case WM_EXITSIZEMOVE:
			// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
			// Here we reset everything based on the new window dimensions.
			_appPaused = false;
			_resizing = false;
			_timer.Start();
			OnResize();
			break;
		case WM_DESTROY:
			// WM_DESTROY is sent when the window is being destroyed.
			PostQuitMessage(0);
			break;
		case WM_MENUCHAR:
			// The WM_MENUCHAR message is sent when a menu is active and the user presses
			// a key that does not correspond to any mnemonic or accelerator key.
			// Don't beep when we alt-enter
			return MAKELRESULT(0, MNC_CLOSE);
		case WM_GETMINMAXINFO:
			// Catch this message so to prevent the window from becoming too small.
			// Triggered when moving or resizing the window
			((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
			((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
			break;
		case WM_LBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_RBUTTONDOWN:
			OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			break;
		case WM_LBUTTONUP:
		case WM_MBUTTONUP:
		case WM_RBUTTONUP:
			OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			break;
		case WM_MOUSEMOVE:
			OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			break;
		case WM_KEYUP:
			if (wParam == VK_ESCAPE) {
				PostQuitMessage(0);
			} else if ((int)wParam == VK_F2) {
				Set4xMsaaState(!_4xMsaaState);
			}
			break;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

D3DApp* D3DApp::_instance = nullptr;
D3DApp* D3DApp::GetApp() {
	return _instance;
}

D3DApp::D3DApp(HINSTANCE hInstance) : _appInstance(hInstance) {
	assert(_instance == nullptr);

	_instance = this;
}

D3DApp::~D3DApp() {
	if (_device != nullptr) {
		FlushCommandQueue();
	}
}

bool D3DApp::Initialize() {
	if (!InitMainWindow()) {
		return false;
	}

	if (!InitDirect3D()) {
		return false;
	}

	// Do the initial resize code.
	OnResize();

	return true;
}

bool D3DApp::InitMainWindow() {
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = _appInstance;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = L"MainWnd";

	if (!RegisterClass(&wc)) {
		MessageBox(0, L"RegisterClass Failed.", 0, 0);
		return false;
	}

	// Compute window rectangle dimensions based on requested client area dimensions.
	RECT R = {0, 0, _clientWidth, _clientHeight};
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;

	_mainWnd = CreateWindow(
		L"MainWnd",
		_mainWndCaption.c_str(),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		width, height,
		0, 0,
		_appInstance,
		0
	);

	if (!_mainWnd) {
		MessageBox(0, L"CreateWindow Failed.", 0, 0);
		return false;
	}

	ShowWindow(_mainWnd, SW_SHOW);
	UpdateWindow(_mainWnd);

	return true;
}

bool D3DApp::InitDirect3D() {

#if defined(DEBUG) || defined (_DEBUG)
	// Enable the D3D12 debug layer.
	ComPtr<ID3D12Debug> debugController;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
	debugController->EnableDebugLayer();
#endif

	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory)));

	//
	// Try to create hardware device.
	//

	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_2,
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	HRESULT hardwareResult;
	for (auto level : levels) {
		hardwareResult = D3D12CreateDevice(
			nullptr,		// default
			level,
			IID_PPV_ARGS(&_device)
		);
		if (hardwareResult == S_OK) {
			break;
		}
	}

	// Fallback to WARP device.
	if (FAILED(hardwareResult)) {
		ComPtr<IDXGIAdapter> pWarpAdapter;
		ThrowIfFailed(_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter)));

		for (auto level : levels) {
			ThrowIfFailed(D3D12CreateDevice(pWarpAdapter.Get(), level, IID_PPV_ARGS(&_device)));
		}
	}

	//
	// Create Fence
	//

	ThrowIfFailed(_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence)));

	// Save the size of view.
	_rtvDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	_dsvDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	_cbvSrvUavDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// Chcek 4X MSAA quality support for our back buffer format.
	// All Direct3D 11 capable devices support 4X MSAA for all render
	// target formats, so we only need to check quality support.
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
	msQualityLevels.Format = _backBufferFormat;
	msQualityLevels.SampleCount = 4;
	msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	msQualityLevels.NumQualityLevels = 0;
	ThrowIfFailed(_device->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&msQualityLevels,
		sizeof(msQualityLevels)
	));

	_4xMsaaQuality = msQualityLevels.NumQualityLevels;
	assert(_4xMsaaQuality > 0 && "Unexpected MSAA quality level.");

#ifdef _DEBUG
	LogAdapters();
#endif

	CreateCommandObjects();
	CreateSwapChain();
	CreateRtvAndDsvDescriptorHeaps();

	return true;
}

int D3DApp::Run() {
	MSG msg = {0};

	_timer.Reset();

	while (msg.message != WM_QUIT) {
		// If there are Window messages then process them.
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		// Otherwise, do animation/game stuff.
		else {
			_timer.Tick();

			if (!_appPaused) {
				CalculateFrameStats();
				Update(_timer);
				Draw(_timer);
			} else {
				Sleep(100);
			}
		}
	}

	return (int)msg.wParam;
}

void D3DApp::CreateCommandObjects() {
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(_cmdQueue.GetAddressOf())));

	ThrowIfFailed(_device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(_cmdAllocator.GetAddressOf()))
	);

	ThrowIfFailed(_device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		_cmdAllocator.Get(),		// Associated command allocator
		nullptr,					// Initial PipelineStateObject
		IID_PPV_ARGS(_cmdList.GetAddressOf()))
	);

	// Start off in a closed state. This is because the first time we refer
	// to the command list we will Reset it, and it needs to be closed before
	// calling Reset.
	_cmdList->Close();
}

void D3DApp::CreateSwapChain() {
	// Release the previous swapchain we will be recreating.
	_swapChain.Reset();

	DXGI_SWAP_CHAIN_DESC sd = {};
	sd.BufferDesc.Width = _clientWidth;
	sd.BufferDesc.Height = _clientHeight;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = _backBufferFormat;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;		// Progressive Scan vs Interlaced Scan
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;						// How the image is stretched relative to the screen.
	sd.SampleDesc.Count = _4xMsaaState ? 4 : 1;
	sd.SampleDesc.Quality = _4xMsaaState ? (_4xMsaaQuality - 1) : 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = _swapChainBufferCount;
	sd.OutputWindow = _mainWnd;
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// Note: Swap chain uses queue to perform flush.
	ThrowIfFailed(_dxgiFactory->CreateSwapChain(
		_cmdQueue.Get(),
		&sd,
		_swapChain.GetAddressOf())
	);
}

void D3DApp::CreateRtvAndDsvDescriptorHeaps() {
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = _swapChainBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(_device->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(_rtvHeap.GetAddressOf()))
	);

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(_device->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(_dsvHeap.GetAddressOf()))
	);
}

void D3DApp::OnResize() {
	assert(_device);
	assert(_swapChain);
	assert(_cmdAllocator);

	// Flush before changing any resources
	FlushCommandQueue();

	ThrowIfFailed(_cmdList->Reset(_cmdAllocator.Get(), nullptr));

	// Release the previous resources we will be recreating.
	for (int i = 0; i < _swapChainBufferCount; ++i) {
		_swapChainBuffer[i].Reset();
	}
	_depthStencilBuffer.Reset();

	// Resize the swap chain.
	ThrowIfFailed(_swapChain->ResizeBuffers(
		_swapChainBufferCount,
		_clientWidth, _clientHeight,
		_backBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
	)
	);

	_currentBackBuffer = 0;

	//CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(_rtvHeap->GetCPUDescriptorHandleForHeapStart());
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(_rtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < _swapChainBufferCount; i++) {
		ThrowIfFailed(_swapChain->GetBuffer(i, IID_PPV_ARGS(&_swapChainBuffer[i])));
		_device->CreateRenderTargetView(_swapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
		//rtvHeapHandle.Offset(1, _rtvDescriptorSize);
		rtvHeapHandle.ptr += _rtvDescriptorSize;
	}

	// Create the depth/stencil buffer and view.
	D3D12_RESOURCE_DESC depthStencilDesc;
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = _clientWidth;
	depthStencilDesc.Height = _clientHeight;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	depthStencilDesc.SampleDesc.Count = _4xMsaaState ? 4 : 1;
	depthStencilDesc.SampleDesc.Quality = _4xMsaaState ? (_4xMsaaQuality - 1) : 0;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear;
	optClear.Format = _depthStencilFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;

	D3D12_HEAP_PROPERTIES dsvHeapProperties = {};
	dsvHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	dsvHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	dsvHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	dsvHeapProperties.CreationNodeMask = 0;
	dsvHeapProperties.VisibleNodeMask = 0;

	ThrowIfFailed(_device->CreateCommittedResource(
		&dsvHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS(_depthStencilBuffer.GetAddressOf())
	));

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = _depthStencilFormat;
	dsvDesc.Texture2D.MipSlice = 0;
	_device->CreateDepthStencilView(_depthStencilBuffer.Get(), &dsvDesc, DepthStencilView());

	// Transition the resource from its initial state to be used as a depth buffer.
	D3D12_RESOURCE_BARRIER resourceBarrier = {};
	resourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	resourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	resourceBarrier.Transition.pResource = _depthStencilBuffer.Get();
	resourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	resourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;

	_cmdList->ResourceBarrier(
		1,
		//&CD3DX12_RESOURCE_BARRIER::Transition(_depthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE)
		&resourceBarrier
	);

	// Execute the resize commands.
	ThrowIfFailed(_cmdList->Close());
	ID3D12CommandList* cmdsLists[] = {_cmdList.Get()};
	_cmdQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until resize is complete.
	FlushCommandQueue();

	// Update the viewport transform to cover the client area.
	_viewport.TopLeftX = 0;
	_viewport.TopLeftY = 0;
	_viewport.Width = static_cast<float> (_clientWidth);
	_viewport.Height = static_cast<float> (_clientHeight);
	_viewport.MinDepth = 0.0f;
	_viewport.MaxDepth = 1.0f;

	_scissorRect = {0, 0, _clientWidth, _clientHeight};
}

void D3DApp::FlushCommandQueue() {
	_currentFence++;

	ThrowIfFailed(_cmdQueue->Signal(_fence.Get(), _currentFence));

	if (_fence->GetCompletedValue() < _currentFence) {
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

		ThrowIfFailed(_fence->SetEventOnCompletion(_currentFence, eventHandle));

		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

ID3D12Resource* D3DApp::CurrentBackBuffer() const {
	return _swapChainBuffer[_currentBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::CurrentBackBufferView() const {
/*
	D3D12_CPU_DESCRIPTOR_HANDLE rtvH = m_RTVHeap->GetCPUDescriptorHandleForHeapStart ();
	rtvH.ptr += m_CurrentBackBuffer * m_RTVDescriptorSize;
	return rtvH;
*/

	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		_rtvHeap->GetCPUDescriptorHandleForHeapStart(),
		_currentBackBuffer,
		_rtvDescriptorSize
	);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::DepthStencilView() const {
	return _dsvHeap->GetCPUDescriptorHandleForHeapStart();
}

void D3DApp::CalculateFrameStats() {
   // Code computes the average frames per second, and also the
   // average time it takes to render one frame. These stats
   // are appended to the window caption bar.

	static int frameCnt = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	if (_timer.TotalTime() - timeElapsed >= 1.0f) {
		float fps = (float)frameCnt;
		float mspf = 1000.0f / fps;

		wstring fpsStr = to_wstring(fps);
		wstring mspfStr = to_wstring(mspf);

		wstring windowText = _mainWndCaption +
			L"    fps: " + fpsStr +
			L"   mspf: " + mspfStr;

		SetWindowText(_mainWnd, windowText.c_str());

		// Reset for next average.
		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}

HINSTANCE D3DApp::AppInstance() const {
	return _appInstance;
}

HWND D3DApp::MainWnd() const {
	return _mainWnd;
}

float D3DApp::AspectRatio() const {
	return static_cast<float> (_clientWidth) / _clientHeight;
}

bool D3DApp::Get4xMsaaState() const {
	return _4xMsaaState;
}

void D3DApp::Set4xMsaaState(bool value) {
	if (_4xMsaaState != value) {
		_4xMsaaState = value;

		// Recreate the swapchain and buffers with new multisample settings
		CreateSwapChain();
		OnResize();
	}
}

void D3DApp::LogAdapters() {
	UINT i = 0;
	IDXGIAdapter* adapter = nullptr;
	std::vector<IDXGIAdapter*> adapterList;
	while (_dxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND) {
		DXGI_ADAPTER_DESC desc;
		adapter->GetDesc(&desc);

		std::wstring text = L"***Adapter: ";
		text += desc.Description;
		text += L"\n";

		OutputDebugString(text.c_str());

		adapterList.push_back(adapter);

		++i;
	}

	for (size_t i = 0; i < adapterList.size(); ++i) {
		LogAdapterOutputs(adapterList[i]);
		ReleaseCom(adapterList[i]);
	}
}

void D3DApp::LogAdapterOutputs(IDXGIAdapter* adapter) {
	UINT i = 0;
	IDXGIOutput* output = nullptr;
	while (adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND) {
		DXGI_OUTPUT_DESC desc;
		output->GetDesc(&desc);

		std::wstring text = L"***Output: ";
		text += desc.DeviceName;
		text += L"\n";
		OutputDebugString(text.c_str());

		LogOutputDisplayModes(output, _backBufferFormat);

		ReleaseCom(output);

		++i;
	}
}

void D3DApp::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format) {
	UINT count = 0;
	UINT flags = 0;

	// Call with nullptr to get list count.
	output->GetDisplayModeList(format, flags, &count, nullptr);

	std::vector<DXGI_MODE_DESC> modeList(count);
	output->GetDisplayModeList(format, flags, &count, &modeList[0]);

	for (auto& x : modeList) {
		UINT n = x.RefreshRate.Numerator;
		UINT d = x.RefreshRate.Denominator;
		std::wstring text =
			L"Width = " + std::to_wstring(x.Width) + L" " +
			L"Height = " + std::to_wstring(x.Height) + L" " +
			L"Refresh = " + std::to_wstring(n) + L"/" + std::to_wstring(d) +
			L"\n";

		OutputDebugString(text.c_str());
	}
}
