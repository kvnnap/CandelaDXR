#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <DirectXMath.h>

#include <vector>
#include <cstdint>

namespace candela::directx
{
	class DXUtil {
	public:
		DXUtil() = delete;
		~DXUtil() = delete;

		// Helper methods
		static void enableDebugLayer();
		static void setupDebugLayer(Microsoft::WRL::ComPtr<ID3D12Device> pDevice, bool breakEnabled);
		static bool checkTearingSupport(Microsoft::WRL::ComPtr<IDXGIFactory> dxgiFactory);

		static Microsoft::WRL::ComPtr<IDXGIAdapter> getAdapterLatestFeatureLevel(Microsoft::WRL::ComPtr<IDXGIFactory> dxgiFactory, D3D_FEATURE_LEVEL* featureLevel, bool useWarp = false, std::uint32_t adapterIndex = 0);
		static std::vector<Microsoft::WRL::ComPtr<IDXGIAdapter>> getAdapters(Microsoft::WRL::ComPtr<IDXGIFactory> dxgiFactory, D3D_FEATURE_LEVEL featureLevel, bool useWarp = false);
		static Microsoft::WRL::ComPtr<ID3D12Device> createDeviceFromAdapter(Microsoft::WRL::ComPtr<IDXGIAdapter> adapter, D3D_FEATURE_LEVEL featureLevel);
		static Microsoft::WRL::ComPtr<IDXGIFactory> createDXGIFactory(bool debugEnabled);
		static Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> createDescriptorHeap(Microsoft::WRL::ComPtr<ID3D12Device> device, UINT count, D3D12_DESCRIPTOR_HEAP_TYPE type, bool shaderVisible = false);
		static Microsoft::WRL::ComPtr<IDXGISwapChain> createSwapChain(Microsoft::WRL::ComPtr<IDXGIFactory> dxgiFactory, Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue, HWND hWnd, UINT numBuffers);

		static std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> createRenderTargetViews(
			Microsoft::WRL::ComPtr<ID3D12Device> device,
			Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap,
			Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain,
			UINT numRTV);

		static std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> createDepthStencilView(
			Microsoft::WRL::ComPtr<ID3D12Device> device,
			Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> depthDescriptorHeap,
			UINT winWidth, UINT winHeight,
			UINT numDSV);

		static Microsoft::WRL::ComPtr<ID3D12Resource> createCommittedResource(Microsoft::WRL::ComPtr<ID3D12Device> device, D3D12_HEAP_TYPE heapType, UINT64 size, D3D12_RESOURCE_STATES resourceState, D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE);
		static Microsoft::WRL::ComPtr<ID3D12Resource> createTextureCommittedResource(Microsoft::WRL::ComPtr<ID3D12Device> device, D3D12_HEAP_TYPE heapType, UINT64 width, UINT height, D3D12_RESOURCE_STATES resourceState, D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);

		static Microsoft::WRL::ComPtr<ID3D12Resource> uploadDataToDefaultHeap(Microsoft::WRL::ComPtr<ID3D12Device> device, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> pCommandList, Microsoft::WRL::ComPtr<ID3D12Resource>& tempResource, const void* ptData, std::size_t dataSize, D3D12_RESOURCE_STATES finalState);
		static Microsoft::WRL::ComPtr<ID3D12Resource> uploadTextureDataToDefaultHeap(Microsoft::WRL::ComPtr<ID3D12Device> device, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> pCommandList, Microsoft::WRL::ComPtr<ID3D12Resource>& tempResource, const void* ptData, std::size_t width, std::size_t height, std::size_t sizePerPixel, DXGI_FORMAT format, D3D12_RESOURCE_STATES finalState);

		static void updateDataInDefaultHeap(Microsoft::WRL::ComPtr<ID3D12Device> device, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> pCommandList, Microsoft::WRL::ComPtr<ID3D12Resource>& resource, Microsoft::WRL::ComPtr<ID3D12Resource>& tempResource, const void* ptData, std::size_t dataSize, D3D12_RESOURCE_STATES previousState, D3D12_RESOURCE_STATES finalState);

		static Microsoft::WRL::ComPtr<ID3D12RootSignature> createRootSignature(Microsoft::WRL::ComPtr<ID3D12Device> device, const D3D12_VERSIONED_ROOT_SIGNATURE_DESC& rootSignatureDesc);

		// RT Stuff
		static Microsoft::WRL::ComPtr<ID3D12Device> createRTDeviceFromAdapter(Microsoft::WRL::ComPtr<IDXGIAdapter> adapter, D3D_FEATURE_LEVEL featureLevel);
		static bool checkDeviceRTSupport(Microsoft::WRL::ComPtr<ID3D12Device> device);
		static bool checkDeviceRTSupport(Microsoft::WRL::ComPtr<IDXGIAdapter> adapter, D3D_FEATURE_LEVEL featureLevel);

		struct AccelerationStructureBuffers
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> pScratch;
			Microsoft::WRL::ComPtr<ID3D12Resource> pResult;
			Microsoft::WRL::ComPtr<ID3D12Resource> pInstanceDesc; // For top-level AS
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
			Microsoft::WRL::ComPtr<ID3D12Device> pDevice,
			Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> pCommandList,
			const std::vector<BottomLevelAccelerationData>& blasData,
			UINT vertexSize);

		static void buildTopLevelAS(
			Microsoft::WRL::ComPtr<ID3D12Device> pDevice,
			Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> pCommandList,
			const std::vector<TopLevelAccelerationData>& instanceData,
			Microsoft::WRL::ComPtr<ID3D12Resource>& tlasTempBuffer,
			bool update,
			AccelerationStructureBuffers& tlasBuffers);
	};
}