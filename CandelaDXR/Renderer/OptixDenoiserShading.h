#pragma once

#include "Drawable.h"

#include <cuda.h>
#include <optix.h>

namespace candela::renderer
{
	class OptixDenoiserShading
		: public Drawable
	{
	public:
		// Inherited via Drawable
		void init(RendererResources* rendererResources, wrl::ComPtr<ID3D12GraphicsCommandList>& pCurrentCommandList, ResourceRegFunction& resRegFn) override;
		void draw(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex) override;
		void accept(IVisitor* visitor) override;
		void onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent) override;
		void onResize() override;
		std::uint32_t getBufferUsage() const override;

	private:
		void initOptix();
		void createContext();
		void createDenoiser();

		RendererResources* rendererResources;
		directx::Resource* diffRadAccumulator;
		//directx::Resource* causticsAccumulator;
		//directx::Resource* diffUnmerged;
		directx::Resource* specRadAccumulator;

		CUstream stream;
		OptixDeviceContext optixContext = nullptr;
		OptixDenoiser denoiser = nullptr;

		CUdeviceptr denoiserState = 0;
		CUdeviceptr denoiserScratch = 0;
		size_t denoiserStateSizeInBytes = 0;
		size_t denoiserScratchSizeInBytes = 0;
	};
}