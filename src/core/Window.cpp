#include "core/Window.h"
#include "core/Input.h"

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

void Window::Open(uint32_t width, uint32_t height)
{
	this->width = width;
	this->height = height;
	unsigned long winStyle = WS_VISIBLE | WS_MINIMIZEBOX | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME;
	hwnd = CreateWindowExW(0, L"Window", L"Window", winStyle, 0, 0, width, height, nullptr, nullptr, GetModuleHandleW(nullptr), this);
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

void Window::AddEventHook(const std::function<void(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)>& fn)
{
	eventHooks.push_back(fn);
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
	{
		win->ProcessEvents(msg, wParam, lParam);
		for (auto& hook : win->eventHooks)
			hook(hwnd, msg, wParam, lParam);
	}

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
	case WM_MOUSEMOVE:
		mouseX = static_cast<int16_t>(LOWORD(lParam));
		mouseY = static_cast<int16_t>(LOWORD(lParam));
		break;
	case WM_KEYDOWN:
	{
		Event::KeyType key = ConvertKeycode(wParam);
		Input::keyPressing[static_cast<int>(key)] = true;
		break;
	}
	case WM_KEYUP:
	{
		Event::KeyType key = ConvertKeycode(wParam);
		Input::keyPressing[static_cast<int>(key)] = false;
		break;
	}
	}
}

auto Window::ConvertKeycode(WPARAM wParam) -> Event::KeyType
{
	//0~9
	if (wParam >= 0x30 && wParam <= 0x39)
		return static_cast<Event::KeyType>(wParam - 0x30 + static_cast<int>(Event::KeyType::Num0));

	//F1~F12
	if (wParam >= VK_F1 && wParam <= VK_F12)
		return static_cast<Event::KeyType>(wParam - VK_F1 + static_cast<int>(Event::KeyType::F1));

	//A~Z
	if (wParam >= 0x41 && wParam <= 0x5A)
		return static_cast<Event::KeyType>(wParam - 0x41 + static_cast<int>(Event::KeyType::A));

	//Left Up Right Down
	if (wParam >= VK_LEFT && wParam <= VK_DOWN)
		return static_cast<Event::KeyType>(wParam - VK_LEFT + static_cast<int>(Event::KeyType::Left));

	//PageUp PageDown End Home
	if (wParam >= VK_PRIOR && wParam <= VK_HOME)
		return static_cast<Event::KeyType>(wParam - VK_PRIOR + static_cast<int>(Event::KeyType::PageUp));

	//Numpad0~9
	if (wParam >= VK_NUMPAD0 && wParam <= VK_NUMPAD9)
		return static_cast<Event::KeyType>(wParam - VK_NUMPAD0 + static_cast<int>(Event::KeyType::Numpad0));

	switch (wParam)
	{
	case VK_SHIFT: return Event::KeyType::Shift;
	case VK_CONTROL: return Event::KeyType::LCtrl;
	case VK_MENU: return Event::KeyType::LAlt;
	case VK_SPACE: return Event::KeyType::Space;
	case VK_BACK: return Event::KeyType::BackSpace;
	case VK_RETURN: return Event::KeyType::Enter;
	case VK_TAB: return Event::KeyType::Tab;
	case VK_ESCAPE: return Event::KeyType::Esc;
	case VK_INSERT: return Event::KeyType::Insert;
	case VK_DELETE: return Event::KeyType::Delete;
	case VK_ADD: return Event::KeyType::NumpadAdd;
	case VK_SUBTRACT: return Event::KeyType::NumpadSubtract;
	case VK_DIVIDE: return Event::KeyType::NumpadDivide;
	case VK_MULTIPLY: return Event::KeyType::NumpadMultiply;
	case VK_DECIMAL: return Event::KeyType::NumpadDecimal;
	case VK_OEM_COMMA: return Event::KeyType::Comma;
	case VK_OEM_PERIOD: return Event::KeyType::Period;
	case VK_OEM_2: return Event::KeyType::Slash;
	case VK_OEM_5: return Event::KeyType::BackSlash;
	case VK_OEM_MINUS: return Event::KeyType::Minus;
	case VK_OEM_PLUS: return Event::KeyType::Equal;
	case VK_OEM_4: return Event::KeyType::LBracket;
	case VK_OEM_6: return Event::KeyType::RBracket;
	case VK_OEM_1: return Event::KeyType::Semicolon;
	case VK_OEM_7: return Event::KeyType::Colon;
	case VK_SNAPSHOT: return Event::KeyType::Print;
	case VK_SCROLL: return Event::KeyType::Scroll;
	case VK_PAUSE: return Event::KeyType::Pause;
	case VK_NUMLOCK: return Event::KeyType::NumLock;
	case VK_OEM_3: return Event::KeyType::Grave;
	case VK_CAPITAL: return Event::KeyType::CapsLock;
	}
	return Event::KeyType::Unknown;
}
