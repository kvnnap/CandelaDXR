#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <functional>

#include "DirectX/Types.h"
#include "IVisitor.h"
#include "IResource.h"

namespace candela::renderer
{
	namespace wrl = Microsoft::WRL;
	
	using ResourceRegFunction = std::function<void(std::unique_ptr<IResource>)>;

	enum BufferUsage : std::uint32_t
	{
		None = 0,
		Radiance = 1,
		Diffuse = 2,
		Specular = 4
	};

	class IDrawable
	{
	public:
		virtual ~IDrawable() = default;
		virtual void init(RendererResources *rendererResources, wrl::ComPtr<ID3D12GraphicsCommandList>& pCurrentCommandList, ResourceRegFunction& resRegFn) = 0;
		virtual void draw(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex) = 0;
		virtual void accept(IVisitor *visitor) = 0;
		// On matrix change
		virtual void onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent) = 0;

		// On window resize
		virtual void onResize() = 0;

		// Is enabled?
		virtual bool isEnabled() const = 0;
		virtual void setEnabled(bool p_enabled) = 0;

		// Accumulation
		virtual bool shouldClearAccumulation() const = 0;
		virtual std::uint32_t getBufferUsage() const = 0;
	};
}