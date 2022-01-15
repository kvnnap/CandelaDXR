#pragma once
#include "Window/WindowsDef.h"
#include <string>
#include <optional>
#include <vector>
#include <functional>

#include "WindowClass.h"
#include "io/ikeywriter.h"
#include "io/imousewriter.h"

namespace candela::ui
{
	class Window
	{
	public:
		// Types
		using WNDCALLBACKFN = std::function<LRESULT(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)>;
		using WNDCALLBACKPT = LRESULT(CALLBACK*)(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

		// Constructors
		Window(const std::string& windowName, int width, int height, feanor::io::IKeyWriter* keyboardWriter = nullptr, feanor::io::IMouseWriter* mouseWriter = nullptr);
		virtual ~Window();

		// Methods
		HWND getHandle() const;
		void setWindowName(const std::string& windowName) const;
		void addWndProcCallback(WNDCALLBACKFN fn);

		// Static
		static std::optional<int> ProcessMessages(bool blocking = false);

	private:
		static LRESULT CALLBACK WndProcSetup(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept;
		static LRESULT CALLBACK WndProcThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept;

		// Private methods
		LRESULT wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept;
		void cleanup(bool destroying = false);

		// Data
		HWND hWnd;
		feanor::io::IKeyWriter* keyboardWriter;
		feanor::io::IMouseWriter* mouseWriter;
		std::vector<WNDCALLBACKFN> callbacks;

		static WindowClass wndClass;
		static uint32_t windowCount;
	};
}
