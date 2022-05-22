#pragma once

#include <vector>
#include <memory>

#include "DirectX/Types.h"
#include "DirectX/CommandQueue.h"
#include "DirectX/RootSignatureManager.h"
#include "DirectX/ShadingTable.h"
#include "DirectX/Resource.h"
#include "Scene/Scene.h"

#include "Mathematics/Types.h"

#include "Camera.h"

#include "Drawable.h"

namespace candela::renderer
{
	namespace wrl = Microsoft::WRL;

	using ResPtrVec = std::vector<directx::Resource*>;

	class RasterShading
		: public Drawable
	{
	public:
		RasterShading(bool computeGBuffer = false);

		void init(RendererResources* rendererResources, wrl::ComPtr<ID3D12GraphicsCommandList>& pCurrentCommandList, ResourceRegFunction& resRegFn) override;
		void draw(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex) override;
		void onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent) override;
		void onResize() override;
		void accept(IVisitor* visitor) override;
		ResPtrVec& getGBuffer();
		UINT getNumRenderTargets() const;
		std::uint32_t getComputeRadiance() const;
		void setComputeRadiance(std::uint32_t cType);
		void setGlobaResourcePrefix(const std::string& prefix);
		void setCamera(Camera* p_camera);
	private:

		RendererResources* rendererResources;

		struct alignas(16) ConstBuff
		{
			DirectX::XMMATRIX ViewPerspective; // Column-Major
			DirectX::XMVECTOR CameraPosition;
			std::uint32_t numLights;
			std::uint32_t computeRadiance;
		} constBuffer;

		D3D12_VERTEX_BUFFER_VIEW bufferViews[3];
		D3D12_INDEX_BUFFER_VIEW indexView;
		D3D12_RECT scissorRect;
		D3D12_VIEWPORT viewport;
		UINT dsvDescriptorSize;
		directx::Resource* pDepthBuffer;

		// G-Buffer
		wrl::ComPtr<ID3D12DescriptorHeap> pGDescriptorHeap;
		ResPtrVec gBuffer;
		const bool computeGBuffer;
		const UINT numRenderTargets;

		wrl::ComPtr<ID3D12DescriptorHeap> pDepthDescriptorHeap;
		wrl::ComPtr<ID3D12Resource> constantBuffer;
		wrl::ComPtr<ID3D12PipelineState> pipelineState;

		std::shared_ptr<directx::RootSignatureManager> rootSignatureManager;
		wrl::ComPtr<ID3D12DescriptorHeap> rootDescriptorHeap;
		wrl::ComPtr<ID3D12RootSignature> rootSignature;

		std::string globalPrefix;
		Camera* camera;
	};
}