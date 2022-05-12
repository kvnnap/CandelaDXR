#pragma once

#include <memory>

#define NOMINMAX
#include <d3d12.h>
#include <wrl/client.h>

namespace candela::directx
{
	class CommandQueue;

	namespace wrl = Microsoft::WRL;

	using DXDevice = wrl::ComPtr<ID3D12Device>;
	using DXResource = wrl::ComPtr<ID3D12Resource>;
	using DXCommandList = wrl::ComPtr<ID3D12GraphicsCommandList>;
	using DXCommandQueue = std::unique_ptr<directx::CommandQueue>;
}