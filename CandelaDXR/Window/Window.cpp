
#include <windowsx.h>
#include <algorithm>

#include "Window.h"
#include "Exception/Exception.h"

using std::nullopt;
using std::string;
using candela::ui::WindowClass;
using candela::ui::Window;

// Static init
WindowClass Window::wndClass = WindowClass("CandelaDXR", Window::WndProcSetup);
uint32_t Window::windowCount{};

LRESULT CALLBACK Window::WndProcSetup(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept
{
	if (msg == WM_NCCREATE) {
		CREATESTRUCT* createStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
		Window* window = reinterpret_cast<Window*>(createStruct->lpCreateParams);

		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
		SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(Window::WndProcThunk));

		return window->wndProc(hwnd, msg, wParam, lParam);
	}
	else {
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
}

LRESULT CALLBACK Window::WndProcThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept
{
	Window* window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
	return window->wndProc(hwnd, msg, wParam, lParam);
}

bool Window::getCorrectedRect(int width, int height, long dwStyle, RECT &rect) const noexcept
{
	rect = {};
	rect.right = width;
	rect.bottom = height;
	return AdjustWindowRect(&rect, dwStyle, false) != 0;
}

long Window::getWindowStyle() const
{
	return GetWindowLong(hWnd, GWL_STYLE);
}

std::optional<int> Window::ProcessMessages(bool blocking)
{
	MSG msg = {};
	while (blocking ? GetMessage(&msg, NULL, 0, 0) : PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
			return static_cast<int>(msg.wParam);

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return blocking ? std::optional<int>(static_cast<int>(msg.wParam)) : nullopt;
}

LRESULT Window::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept
{
	for (auto& callback : callbacks)
	{
		LRESULT retValue;
		if (callback(hwnd, msg, wParam, lParam, retValue))
			return retValue;
	}
	
	switch (msg)
	{
	case WM_DESTROY:
		cleanup(true);
		return 0;
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		if (keyboardWriter && !(lParam & (1 << 30)))
			keyboardWriter->pressKey(static_cast<uint8_t>(wParam));
		return 0;
	case WM_KEYUP:
	case WM_SYSKEYUP:
		if (keyboardWriter)
			keyboardWriter->depressKey(static_cast<uint8_t>(wParam));
		if (wParam == VK_ESCAPE)
			PostMessage(hwnd, WM_CLOSE, 0, 0);
		return 0;
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
		if (mouseWriter)
		{
			if (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN) {
				mouseWriter->pressKey(msg == WM_LBUTTONDOWN ? 0 : msg == WM_RBUTTONDOWN ? 1 : 2);
			}
			else if (msg == WM_LBUTTONUP || msg == WM_RBUTTONUP || msg == WM_MBUTTONUP) {
				mouseWriter->depressKey(msg == WM_LBUTTONUP ? 0 : msg == WM_RBUTTONUP ? 1 : 2);
			}
		}
		[[fallthrough]]; // C++17 syntax: Annotate intentional fallthrough
	case WM_MOUSEMOVE:
	case WM_MOUSEWHEEL:
		if (mouseWriter)
		{
			mouseWriter->updatePosition(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			mouseWriter->scroll(GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA);
		}

		return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void Window::cleanup(bool destroying)
{
	if (!hWnd)
		return;

	if (!destroying)
		DestroyWindow(hWnd);
	hWnd = HWND();
	if(--windowCount == 0)
		PostQuitMessage(0);
}

Window::Window(const string& windowName, int width, int height, feanor::io::IKeyWriter* keyboardWriter, feanor::io::IMouseWriter* mouseWriter, bool resizeable)
	: hWnd(), keyboardWriter(keyboardWriter), mouseWriter(mouseWriter)
{
	auto dwStyle = resizeable ? WS_OVERLAPPEDWINDOW : WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;

	RECT rect;
	if (!getCorrectedRect(width, height, dwStyle, rect))
		ThrowException("Cannot adjust client area");

	HINSTANCE hInstance = GetModuleHandle(NULL);

	hWnd = CreateWindowEx(NULL,
		wndClass.getClassName().c_str(),
		windowName.c_str(),
		dwStyle,
		CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,
		NULL,
		NULL,
		hInstance,
		this);

	if (!hWnd)
	{
		cleanup();
		ThrowException("Error creating window");
	}

	++windowCount;
	ShowWindow(hWnd, SW_SHOW);
}

Window::~Window()
{
	cleanup();
}

HWND Window::getHandle() const
{
	return hWnd;
}

void candela::ui::Window::setWindowName(const std::string& windowName) const
{
	SetWindowText(hWnd, windowName.c_str());
}

void Window::setClientWindowSize(int width, int height) const
{
	RECT rect;
	if (!getCorrectedRect(width, height, getWindowStyle(), rect))
		ThrowException("Cannot adjust client area");
	if (!SetWindowPos(hWnd, HWND_TOP, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOOWNERZORDER | SWP_NOMOVE))
		ThrowException("Cannot resize window");
}

void Window::setAspectRatio(float aspectRatio) const
{
	RECT rect;
	GetClientRect(hWnd, &rect);
	auto clientHeight = static_cast<int>(rect.bottom - rect.top);
	auto clientWidth = static_cast<int>(aspectRatio * clientHeight);
	setClientWindowSize(clientWidth, clientHeight);
}

void Window::getClientWindowSize(int& width, int& height) const
{
	RECT rect;
	GetClientRect(hWnd, &rect);
	width = static_cast<int>(rect.right - rect.left);
	height = static_cast<int>(rect.bottom - rect.top);
}

void Window::addWndProcCallback(WNDCALLBACKFN a)
{
	callbacks.push_back(move(a));
}

void Window::addWndProcCallback(WNDCALLBACKFN2 fn, LRESULT acceptedValue)
{
	callbacks.push_back([fn = std::move(fn), acceptedValue](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT& result) -> bool {
		result = fn(hwnd, msg, wParam, lParam);
		return result == acceptedValue;
	});
}
