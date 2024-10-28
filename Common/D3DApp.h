#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "D3DUtil.h"
#include "GameTimer.h"

#pragma comment (lib, "d3dcompiler.lib")
#pragma comment (lib, "d3d12.lib")
#pragma comment (lib, "dxgi.lib")

class D3DApp {
protected:
	D3DApp(HINSTANCE hInstance);
	D3DApp(const D3DApp& rhs) = delete;
	D3DApp& operator = (const D3DApp& rhs) = delete;
	virtual ~D3DApp();

public:
	static D3DApp* GetApp();

	HINSTANCE AppInstance() const;
	HWND MainWnd() const;
	float AspectRatio() const;

	bool Get4xMsaaState() const;
	void Set4xMsaaState(bool value);

	int Run();

	virtual bool Initialize();
	virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
	virtual void CreateRtvAndDsvDescriptorHeaps();
	virtual void OnResize();
	virtual void Update(const GameTimer& gt) = 0;
	virtual void Draw(const GameTimer& gt) = 0;

	virtual void OnMouseDown(WPARAM btnState, int x, int y) {}
	virtual void OnMouseUp(WPARAM btnState, int x, int y) {}
	virtual void OnMouseMove(WPARAM btnState, int x, int y) {}

protected:
	bool InitMainWindow();
	bool InitDirect3D();
	void CreateCommandObjects();
	void CreateSwapChain();

	void FlushCommandQueue();

	ID3D12Resource* CurrentBackBuffer() const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

	void CalculateFrameStats();

	void LogAdapters();
	void LogAdapterOutputs(IDXGIAdapter* adapter);
	void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);

protected:
	static D3DApp* _instance;

	HINSTANCE _appInstance = nullptr;	// application instance handle
	HWND _mainWnd = nullptr;			// main window handle
	bool _appPaused = false;			// is the application paused?
	bool _minimized = false;			// is the application minimized?
	bool _maximized = false;			// is the application maximized?
	bool _resizing = false;				// are the resize bars being dragged?
	bool _fullscreenState = false;	    // fullscreen enabled

	bool _4xMsaaState = false;			// 4X MSAA enabled
	UINT _4xMsaaQuality = 0;			// quality level of 4X MSAA

	GameTimer _timer;

	Microsoft::WRL::ComPtr<IDXGIFactory4> _dxgiFactory;
	Microsoft::WRL::ComPtr<ID3D12Device> _device;
	Microsoft::WRL::ComPtr<IDXGISwapChain> _swapChain;

	Microsoft::WRL::ComPtr<ID3D12Fence> _fence;
	UINT64 _currentFence = 0;

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> _cmdQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> _cmdAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> _cmdList;

	static const int _swapChainBufferCount = 2;
	int _currentBackBuffer = 0;
	Microsoft::WRL::ComPtr<ID3D12Resource> _swapChainBuffer[_swapChainBufferCount];
	Microsoft::WRL::ComPtr<ID3D12Resource> _depthStencilBuffer;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _rtvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _dsvHeap;

	D3D12_VIEWPORT _viewport;
	D3D12_RECT _scissorRect;

	UINT _rtvDescriptorSize = 0;			// render target view size
	UINT _dsvDescriptorSize = 0;			// depth and stencil view size
	UINT _cbvSrvUavDescriptorSize = 0;		// constant view, shader resource view,  size

	// Deviced class should set these in derived constructor to customize starting values.
	std::wstring _mainWndCaption = L"DX APP";
	D3D_DRIVER_TYPE _d3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
	DXGI_FORMAT _backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT _depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	int _clientWidth = 800;
	int _clientHeight = 600;
};

