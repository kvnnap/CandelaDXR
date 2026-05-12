#pragma once

#include <cstdint>

#include "DirectX/Types.h"
#include "RendererResources.h"
#include "ChangeEvent.h"

namespace candela::renderer
{
	namespace wrl = Microsoft::WRL;

	class IResource
	{
	public:
		virtual ~IResource() = default;
		virtual void init(RendererResources* rendererResources, directx::DXCommandList& pCurrentCommandList) = 0;
		virtual void onChange(directx::DXCommandList pCurrentCommandList, std::uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent) = 0;
	};
}