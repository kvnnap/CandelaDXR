#pragma once

#include <vector>
#include <memory>

#include "DirectX/Types.h"
#include "DirectX/CommandQueue.h"
#include "DirectX/RootSignatureManager.h"
#include "DirectX/Resource.h"
#include "Scene/Scene.h"

#include "Mathematics/Types.h"
#include "Mathematics/AABB.h"

#include "Camera.h"

#include "Drawable.h"

namespace candela::renderer
{
	class ExternalObjectDebugShading
		: public Drawable
	{
	public:
		ExternalObjectDebugShading();

		void init(RendererResources* rendererResources, wrl::ComPtr<ID3D12GraphicsCommandList>& pCurrentCommandList, ResourceRegFunction& resRegFn) override;
		void draw(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex) override;
		void onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent) override;
		void onResize() override;
		void accept(IVisitor* visitor) override;
		std::uint32_t getBufferUsage() const override;
		void setEnabled(bool p_enabled) override;
		void setVertices(std::vector<mathematics::Vector3>&& vertices);

		void setDisplaySceneAabb(bool p_disp);
		bool getDisplaySceneAabb() const;

	private:
		void updateBuffer(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList);
		void appendAabb(const mathematics::AABB& aabb);
		RendererResources* rendererResources;

		struct alignas(16) ConstBuff
		{
			DirectX::XMMATRIX ViewPerspective; // Column-Major
		} constBuffer;

		D3D12_VERTEX_BUFFER_VIEW bufferView;
		D3D12_RECT scissorRect;
		D3D12_VIEWPORT viewport;
		UINT dsvDescriptorSize;
		directx::Resource* pDepthBuffer;

		std::unique_ptr<directx::Resource> vertexBuffer;

		wrl::ComPtr<ID3D12DescriptorHeap> pDepthDescriptorHeap;
		wrl::ComPtr<ID3D12Resource> constantBuffer;
		wrl::ComPtr<ID3D12PipelineState> pipelineState;

		std::shared_ptr<directx::RootSignatureManager> rootSignatureManager;
		//wrl::ComPtr<ID3D12DescriptorHeap> rootDescriptorHeap;
		wrl::ComPtr<ID3D12RootSignature> rootSignature;

		// Vertex Data
		std::vector<mathematics::Vector3> vertices;

		bool needsUpdate;
		bool displaySceneAabb;
	};
}
