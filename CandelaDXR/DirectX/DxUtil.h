#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <DirectXMath.h>

#include <vector>
#include <cstdint>
#include <memory>

#include "Resource.h"

namespace candela::directx
{
	class DXUtil {
	public:
		DXUtil() = delete;
		~DXUtil() = delete;

		// Helper methods
		static void enableDebugLayer();
		static void setupDebugLayer(DXDevice pDevice, bool breakEnabled);
		static bool checkTearingSupport(Microsoft::WRL::ComPtr<IDXGIFactory> dxgiFactory);

		static Microsoft::WRL::ComPtr<IDXGIAdapter> getAdapterLatestFeatureLevel(Microsoft::WRL::ComPtr<IDXGIFactory> dxgiFactory, D3D_FEATURE_LEVEL* featureLevel, bool useWarp = false, std::uint32_t adapterIndex = 0);
		static std::vector<Microsoft::WRL::ComPtr<IDXGIAdapter>> getAdapters(Microsoft::WRL::ComPtr<IDXGIFactory> dxgiFactory, D3D_FEATURE_LEVEL featureLevel, bool useWarp = false);
		static DXDevice createDeviceFromAdapter(Microsoft::WRL::ComPtr<IDXGIAdapter> adapter, D3D_FEATURE_LEVEL featureLevel);
		static Microsoft::WRL::ComPtr<IDXGIFactory> createDXGIFactory(bool debugEnabled);
		static Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> createDescriptorHeap(DXDevice device, UINT count, D3D12_DESCRIPTOR_HEAP_TYPE type, bool shaderVisible = false);
		static Microsoft::WRL::ComPtr<IDXGISwapChain> createSwapChain(Microsoft::WRL::ComPtr<IDXGIFactory> dxgiFactory, Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue, HWND hWnd, UINT numBuffers, UINT width = 0u, UINT height = 0u);

		static std::vector<DXResource> createRenderTargetViews(
			DXDevice device,
			Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap,
			Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain,
			UINT numRTV);

		static std::vector<std::shared_ptr<Resource>> createRenderTargetViewsEx(
			DXDevice device,
			Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap,
			Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain,
			std::vector<DXResource>& textureTargets,
			UINT numRTV);

		static std::vector<std::shared_ptr<Resource>> createRenderTargetViewsEx(
			DXDevice device,
			Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap,
			Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain,
			const std::vector<Resource*>& textureTargets,
			UINT numRTV);

		static std::vector<DXResource> createDepthStencilView(
			DXDevice device,
			Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> depthDescriptorHeap,
			UINT winWidth, UINT winHeight,
			UINT numDSV);

		static DXResource createCommittedResource(DXDevice device, D3D12_HEAP_TYPE heapType, UINT64 size, D3D12_RESOURCE_STATES resourceState, D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE, D3D12_CLEAR_VALUE* clearValue = nullptr);
		static DXResource createTextureCommittedResource(DXDevice device, D3D12_HEAP_TYPE heapType, UINT64 width, UINT height, D3D12_RESOURCE_STATES resourceState, D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_CLEAR_VALUE* clearValue = nullptr);

		static DXResource uploadDataToDefaultHeap(DXDevice device, DXCommandList pCommandList, DXResource& tempResource, const void* ptData, std::size_t dataSize, D3D12_RESOURCE_STATES finalState);
		static DXResource uploadTextureDataToDefaultHeap(DXDevice device, DXCommandList pCommandList, DXResource& tempResource, const void* ptData, std::size_t width, std::size_t height, std::size_t bitsPerPixel, DXGI_FORMAT format, D3D12_RESOURCE_STATES finalState);

		static void updateDataInDefaultHeap(DXDevice device, DXCommandList pCommandList, DXResource& resource, DXResource& tempResource, const void* ptData, std::size_t dataSize, D3D12_RESOURCE_STATES previousState, D3D12_RESOURCE_STATES finalState);

		static Microsoft::WRL::ComPtr<ID3D12RootSignature> createRootSignature(DXDevice device, const D3D12_VERSIONED_ROOT_SIGNATURE_DESC& rootSignatureDesc);

		// RT Stuff
		static DXDevice createRTDeviceFromAdapter(Microsoft::WRL::ComPtr<IDXGIAdapter> adapter, D3D_FEATURE_LEVEL featureLevel);
		static bool checkDeviceRTSupport(DXDevice device);
		static bool checkDeviceRTSupport(Microsoft::WRL::ComPtr<IDXGIAdapter> adapter, D3D_FEATURE_LEVEL featureLevel);

		struct AccelerationStructureBuffers
		{
			DXResource pScratch;
			DXResource pResult;
			DXResource pInstanceDesc; // For top-level AS
		};

		struct TopLevelAccelerationData
		{
			DirectX::XMFLOAT3X4 transform;
			std::size_t instanceId;
			AccelerationStructureBuffers blasBuffer;
		};

		struct BottomLevelAccelerationData
		{
			D3D12_GPU_VIRTUAL_ADDRESS vertexBuffer;
			D3D12_GPU_VIRTUAL_ADDRESS indexBuffer;
			UINT vertexCount;
			UINT indexCount;
		};

		// Vertex buffer must be in a readable state
		// The bottom level AS deals with objects at the local level
		static AccelerationStructureBuffers createBottomLevelAS(
			DXDevice pDevice,
			DXCommandList pCommandList,
			const std::vector<BottomLevelAccelerationData>& blasData,
			UINT vertexSize);

		static void buildTopLevelAS(
			DXDevice pDevice,
			DXCommandList pCommandList,
			const std::vector<TopLevelAccelerationData>& instanceData,
			DXResource& tlasTempBuffer,
			bool update,
			AccelerationStructureBuffers& tlasBuffers);

		static D3D12_STATIC_SAMPLER_DESC getDefaultSamplerDesc();

		// Queries
		static Microsoft::WRL::ComPtr<ID3D12QueryHeap> createQueryHeap(DXDevice pDevice, UINT heapSize);
		static Microsoft::WRL::ComPtr<ID3DBlob> LoadShaderResource(const char* path);
	};
}