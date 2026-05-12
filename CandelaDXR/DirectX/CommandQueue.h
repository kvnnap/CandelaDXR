#pragma once

#include "Window/WindowsDef.h"
#include "DirectX/Types.h"
#include <DirectXMath.h>
#include <cstdint>
#include <queue>

namespace candela::directx
{
	class CommandQueue
	{
	public:
		CommandQueue(DXDevice pDevice, D3D12_COMMAND_LIST_TYPE listType);
		virtual ~CommandQueue();

		Microsoft::WRL::ComPtr<ID3D12CommandQueue> getCommandQueue() const;
		DXCommandList getCommandList();
		std::uint64_t executeCommandList(DXCommandList commandList);
		ID3D12CommandAllocator* getCommandAllocator(DXCommandList commandList);
		std::uint64_t signal();
		bool isFenceComplete(std::uint64_t fenceValue);
		void waitForFenceValue(std::uint64_t fenceValue);
		void flush();

		static void waitForEvent(HANDLE handleEvent);

	protected:
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> createCommandAllocator();
		DXCommandList createCommandList(Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator);

	private:

		struct CommandAllocatorEntry {
			std::uint64_t fenceValue;
			Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
		};

		using CommandAllocatorQueue = std::queue<CommandAllocatorEntry>;
		using CommandListQueue = std::queue<DXCommandList>;

		//Data
		D3D12_COMMAND_LIST_TYPE listType;
		DXDevice pDevice;
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