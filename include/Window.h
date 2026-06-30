#pragma once

#include <Windows.h>

class Window
{
public:
	Window();

	void Open();
	void Close();
	void Update();

	auto IsOpen() const -> bool { return hwnd != nullptr; }
	auto GetInstance() const -> HMODULE { return instance; }
	auto GetHWND() const -> HWND { return hwnd; }
private:
	static LRESULT CALLBACK EventHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	void ProcessEvents(UINT msg, WPARAM wParam, LPARAM lParam);
private:
	HMODULE instance = nullptr;
	HWND hwnd = nullptr;
};