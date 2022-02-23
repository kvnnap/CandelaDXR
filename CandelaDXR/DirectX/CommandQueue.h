#pragma once

#define NOMINMAX
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <cstdint>
#include <queue>

namespace candela::directx
{
	class CommandQueue
	{
	public:
		CommandQueue(Microsoft::WRL::ComPtr<ID3D12Device> pDevice, D3D12_COMMAND_LIST_TYPE listType);
		virtual ~CommandQueue();

		Microsoft::WRL::ComPtr<ID3D12CommandQueue> getCommandQueue() const;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> getCommandList();
		std::uint64_t executeCommandList(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList);
		std::uint64_t signal();
		bool isFenceComplete(std::uint64_t fenceValue);
		void waitForFenceValue(std::uint64_t fenceValue);
		void flush();

		static void waitForEvent(HANDLE handleEvent);

	protected:
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> createCommandAllocator();
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> createCommandList(Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator);

	private:

		struct CommandAllocatorEntry {
			std::uint64_t fenceValue;
			Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
		};

		using CommandAllocatorQueue = std::queue<CommandAllocatorEntry>;
		using CommandListQueue = std::queue<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>>;

		//Data
		D3D12_COMMAND_LIST_TYPE listType;
		Microsoft::WRL::ComPtr<ID3D12Device> pDevice;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> pCommandQueue;

		// Queues

		// Command allocators - only use one per in-flight render frame
		CommandAllocatorQueue commandAllocatorQueue;

		// Submit a command list this to the command queue
		CommandListQueue commandListQueue;

		// Synchronization objects
		Microsoft::WRL::ComPtr<ID3D12Fence> pFence;
		uint64_t fenceValue;
		HANDLE fenceEvent;
	};
}