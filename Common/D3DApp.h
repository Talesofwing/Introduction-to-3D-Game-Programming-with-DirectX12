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

	D3DApp (HINSTANCE hInstance);
	D3DApp (const D3DApp& rhs) = delete;
	D3DApp& operator = (const D3DApp& rhs) = delete;
	virtual ~D3DApp ();

public:

	static D3DApp* GetApp ();

	HINSTANCE AppInstance () const;
	HWND MainWnd () const;
	float AspectRatio () const;

	bool Get4xMsaaState () const;
	void Set4xMsaaState (bool value);

	int Run ();

	virtual bool Initialize ();
	virtual LRESULT MsgProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
	virtual void CreateRtvAndDsvDescriptorHeaps ();
	virtual void OnResize ();
	virtual void Update (const GameTimer& gt) = 0;
	virtual void Draw (const GameTimer& gt) = 0;

	virtual void OnMouseDown (WPARAM btnState, int x, int y) {}
	virtual void OnMouseUp (WPARAM btnState, int x, int y) {}
	virtual void OnMouseMove (WPARAM btnState, int x, int y) {}

protected:

	bool InitMainWindow ();
	bool InitDirect3D ();
	void CreateCommandObjects ();
	void CreateSwapChain ();

	void FlushCommandQueue ();

	ID3D12Resource* CurrentBackBuffer () const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView () const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView () const;

	void CalculateFrameStats ();

	void LogAdapters ();
	void LogAdapterOutputs (IDXGIAdapter* adapter);
	void LogOutputDisplayModes (IDXGIOutput* output, DXGI_FORMAT format);

protected:

	static D3DApp* m_Instance;

	HINSTANCE m_AppInstance = nullptr;	// application instance handle
	HWND m_MainWnd = nullptr;			// main window handle
	bool m_AppPaused = false;			// is the application paused?
	bool m_Minimized = false;			// is the application minimized?
	bool m_Maximized = false;			// is the application maximized?
	bool m_Resizing = false;				// are the resize bars being dragged?
	bool m_FullscreenState =  false;		// fullscreen enabled

	bool m_4xMsaaState = false;		// 4X MSAA enabled
	UINT m_4xMsaaQuality = 0;		// quality level of 4X MSAA

	GameTimer m_Timer;

	Microsoft::WRL::ComPtr<IDXGIFactory4> m_DxgiFactory;
	Microsoft::WRL::ComPtr<ID3D12Device> m_Device;
	Microsoft::WRL::ComPtr<IDXGISwapChain> m_SwapChain;

	Microsoft::WRL::ComPtr<ID3D12Fence> m_Fence;
	UINT64 m_CurrentFence = 0;

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_CmdQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_CmdAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_CmdList;

	static const int m_SwapChainBufferCount = 2;
	int m_CurrentBackBuffer = 0;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_SwapChainBuffer[m_SwapChainBufferCount];
	Microsoft::WRL::ComPtr<ID3D12Resource> m_DepthStencilBuffer;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_RTVHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_DSVHeap;

	D3D12_VIEWPORT m_Viewport;
	D3D12_RECT m_ScissorRect;

	UINT m_RtvDescriptorSize = 0;			// render target view size
	UINT m_DsvDescriptorSize = 0;			// depth and stencil view size
	UINT m_CbvSrvUavDescriptorSize = 0;		// constant view, shader resource view,  size

	// Deviced class should set these in derived constructor to customize starting values.
	std::wstring m_MainWndCaption = L"DX APP";
	D3D_DRIVER_TYPE m_D3DDriverType = D3D_DRIVER_TYPE_HARDWARE;
	DXGI_FORMAT m_BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT m_DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	int m_ClientWidth = 800;
	int m_ClientHeight = 600;
};

