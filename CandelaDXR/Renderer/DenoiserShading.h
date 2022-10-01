#pragma once

#include <vector>
#include <memory>
#include <cstdint>
#include <string>

#include "DirectX/Types.h"
#include "DirectX/DXUtil.h"
#include "DirectX/CommandQueue.h"
#include "DirectX/RootSignatureManager.h"
#include "DirectX/ShadingTable.h"
#include "DirectX/Resource.h"
#include "Scene/Scene.h"
#include "Sampler/UniformSampler.h"

#include "Mathematics/Types.h"

#include "Camera.h"

#include "Drawable.h"
#include "RasterShading.h"

#include "NRD.h"


class NrdIntegration;

namespace nri
{
	struct Device;
}

namespace candela::renderer
{
	namespace wrl = Microsoft::WRL;

	struct NriInterface;

	class DenoiserShading
		: public Drawable
	{
	public:
		DenoiserShading();

		~DenoiserShading();

		void init(RendererResources* rendererResources, wrl::ComPtr<ID3D12GraphicsCommandList>& pCurrentCommandList, ResourceRegFunction& resRegFn) override;
		void draw(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex) override;
		void onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent) override;
		void onResize() override;
		void accept(IVisitor* visitor) override;

		nrd::CommonSettings& getCommonSettings();
		nrd::ReblurSettings& getReblurSettings();
		void clearHistory();

	private:

		void compute(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t mode);
		void createShaderResources();
		void setupDenoiser();
		void destroyDenoiser();

		// Common renderer resources
		RendererResources* rendererResources;
		std::unique_ptr<NrdIntegration> NRD;
		nri::Device *nriDevice;
		std::unique_ptr<NriInterface> NRI;

		// Shader stuff
		RasterShading rasterShader;

		// Compute Shader
		// Compute shader
		std::unique_ptr<directx::DescriptorHeap> descHeapManager;
		wrl::ComPtr<ID3D12RootSignature> computeRootSignature;
		wrl::ComPtr<ID3D12PipelineState> computePipelineState;

		// Inputs
		/*
		*  diffuse - get from main radiance signal - tone map before feeding to denoiser
		*  specular - 0
		*  motion vectors - 0
		*  normal - use from normal texture
		*/
		directx::Resource* radAccumulator;
		directx::Resource* albedo; // need to radAccum/albedo --> IN_DIFF_RADIANCE_HITDIST 
		directx::Resource* normal; // Produce (normal, 0.f)  --> IN_NORMAL_ROUGHNESS
		directx::Resource* depth; // 
		directx::Resource* pt_rad; // 
		directx::Resource* in_mv; // Produce (0,0,0)  --> IN_MV
		directx::Resource* in_normal_roughness;
		directx::Resource* in_view_z; // need to linearize from g_buffer
		directx::Resource* in_diff_radiance_hitdist;
		directx::Resource* out_diff_radiance_hitdist;

		nrd::CommonSettings nrdCommonSettings{};
		nrd::ReblurSettings nrdReblurSettings{};

	};
}