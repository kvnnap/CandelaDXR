#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <functional>

#define NOMINMAX
#include <d3d12.h>
#include <wrl/client.h>

#include "DirectX/CommandQueue.h"
#include "DirectX/Resource.h"
#include "Scene/Scene.h"

#include "Mathematics/Types.h"

#include "Camera.h"

namespace candela::renderer
{
	namespace wrl = Microsoft::WRL;

	class AccelerationStructure;

	struct RendererResources
	{
		wrl::ComPtr<ID3D12Device> pDevice;
		wrl::ComPtr<ID3D12Resource> sceneBuffer;
		wrl::ComPtr<ID3D12Resource> materialBuffer;
		wrl::ComPtr<ID3D12Resource> faceAttributeBuffer;
		wrl::ComPtr<ID3D12Resource> lightBuffer;
		wrl::ComPtr<ID3D12Resource> specularBuffer;
		wrl::ComPtr<ID3D12Resource> matrices;
		wrl::ComPtr<ID3D12Resource> normalMatrices;
		std::vector<directx::Resource> textures;
		wrl::ComPtr<ID3D12DescriptorHeap> pRTVDescriptorHeap;
		std::vector<std::shared_ptr<directx::Resource>> pRTVRadBackBuffers;
		directx::CommandQueue *commandQueue;
		mathematics::UVector2 winDimensions;
		UINT numBackBuffers;
		scene::Scene *scene;
		Camera *camera;
		AccelerationStructure* accelerationStructure;
		std::vector<wrl::ComPtr<ID3D12Resource>> initTempBuffers;
		std::vector<std::vector<wrl::ComPtr<ID3D12Resource>>> tempBuffers;
		UINT currentBackBufferIndex;

		directx::DXResource& getTempResource()
		{
			return tempBuffers[currentBackBufferIndex].emplace_back();
		}
	};

	enum class ChangeEvent : std::uint32_t
	{
		Transformation = 0x01,		// Like Matrix Rotation, etc
		SceneUpdate    = 0x02,		// Scene buffer content change but no size change
		SceneChange    = 0x04,		// Scene buffer size change (like num of lights, specs and so on)
		Statistics	   = 0x08		// Update stats only
	};

	using ChangeEvent_t = std::underlying_type<ChangeEvent>::type;

	class RasterShading;
	class LightTracingShading;
	class PathTracingShading;
	class RasterRTShadowsShading;

	class IVisitor
	{
	public:
		virtual ~IVisitor() = default;
		virtual void visit(RasterShading*) = 0;
		virtual void visit(LightTracingShading*) = 0;
		virtual void visit(PathTracingShading*) = 0;
		virtual void visit(RasterRTShadowsShading*) = 0;
	};

	class IResource
	{
	public:
		virtual ~IResource() = default;
		virtual void init(RendererResources* rendererResources, wrl::ComPtr<ID3D12GraphicsCommandList>& pCurrentCommandList) = 0;
		virtual void onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent) = 0;
	};

	using ResourceRegFunction = std::function<void(std::unique_ptr<IResource>)>;

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
	};
}