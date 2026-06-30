#include "Window.h"

#include <iostream>
Window::Window()
{
	instance = GetModuleHandleW(nullptr);

	WNDCLASSW wc{ 0 };
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = &EventHandler;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = instance;
	wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName = nullptr;
	wc.lpszClassName = L"Window";

	RegisterClassW(&wc);
}

void Window::Open()
{
	unsigned long winStyle = WS_VISIBLE | WS_MINIMIZEBOX | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME;
	hwnd = CreateWindowExW(0, L"Window", L"Window", winStyle, 0, 0, 1024, 768, nullptr, nullptr, GetModuleHandleW(nullptr), this);
}

void Window::Close()
{
	if (hwnd == nullptr)
		return;
	DestroyWindow(hwnd);
	hwnd = nullptr;
}

void Window::Update()
{
	MSG msg;
	while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
}

LRESULT Window::EventHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_CREATE)
	{
		Window* const win = reinterpret_cast<Window*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
		win->hwnd = hwnd;

		//이벤트를 전달한 창의 USERDATA의 값을 수정
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(win));
	}

	Window* const win = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
	if (win != nullptr)
		win->ProcessEvents(msg, wParam, lParam);

	if (msg == WM_CLOSE)
		return 0;

	if (msg == WM_SYSCOMMAND)
		if (wParam == SC_KEYMENU)
			return 0;

	return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void Window::ProcessEvents(UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (hwnd == nullptr)
		return;

	switch (msg)
	{
	case WM_CLOSE:
		Close();
		break;
	}
}