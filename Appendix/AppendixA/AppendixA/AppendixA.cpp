#include <windows.h>

HWND mainHwnd = 0;

bool InitWindowsApp(HINSTANCE, int);

int Run();

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nCmdShow) {
	if (!InitWindowsApp(hInstance, nCmdShow))
		return 0;

	return Run();
}

bool InitWindowsApp(HINSTANCE instanceHandle, int show) {
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = instanceHandle;
	wc.hIcon = LoadIcon(0, IDI_SHIELD);
	wc.hCursor = LoadCursor(0, IDC_HAND);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = L"BasicWndClass";

	if (!RegisterClass(&wc)) {
		MessageBox(0, L"RegisterClass FAILED", 0, 0);
		return false;
	}

	mainHwnd = CreateWindow(
		L"BasicWndClass",
		L"Win32Basic",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		0,
		0,
		instanceHandle,
		0
	);

	if (mainHwnd == 0) {
		MessageBox(0, L"CreateWindow FAILED", 0, 0);
		return false;
	}

	ShowWindow(mainHwnd, show);
	UpdateWindow(mainHwnd);

	return true;
}

int Run() {
	MSG msg = {0};

	BOOL bRet = 1;
	while ((bRet = GetMessage(&msg, 0, 0, 0)) != 0) {
		if (bRet == -1) {
			MessageBox(0, L"GetMessage FAILED", L"Error", MB_OK);
			return false;
		} else {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_CREATE:
			MessageBox(0, L"Created", L"Create Window", MB_OK);
			return 0;
		case WM_LBUTTONDOWN:
			MessageBox(0, L"Hello World", L"Hello", MB_OK);
			return 0;
		case WM_KEYDOWN:
			if (wParam == VK_ESCAPE) {
				int msgboxID = MessageBox(0, L"Close?", L"Close", MB_YESNO);
				switch (msgboxID) {
					case IDYES:
						DestroyWindow(mainHwnd);
						break;
				}
			}
			return 0;
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}
