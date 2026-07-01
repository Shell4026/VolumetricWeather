#pragma once
#include <Windows.h>
#include <functional>
class Window
{
public:
	Window();

	void Open();
	void Close();
	void Update();
	void AddEventHook(const std::function<void(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)>& fn);

	auto IsOpen() const -> bool { return hwnd != nullptr; }
	auto GetInstance() const -> HMODULE { return instance; }
	auto GetHWND() const -> HWND { return hwnd; }
	auto GetWidth() const -> uint32_t { return width; }
	auto GetHeight() const -> uint32_t { return height; }
	auto GetMouseX() const -> int { return mouseX; }
	auto GetMouseY() const -> int { return mouseY; }
private:
	static LRESULT CALLBACK EventHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	void ProcessEvents(UINT msg, WPARAM wParam, LPARAM lParam);
private:
	HMODULE instance = nullptr;
	HWND hwnd = nullptr;

	uint32_t width = 1024;
	uint32_t height = 768;

	int mouseX = 0;
	int mouseY = 0;

	std::vector<std::function<void(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)>> eventHooks;
};