#pragma once

#include "ILightTracingComponent.h"

namespace candela::renderer
{
	class LTOptimisedComponent
		: public ILightTracingComponent
	{
	public:
		LTOptimisedComponent();

		void setCausticsRatio(float p_causticsRatio);
		float getCausticsRatio() const;

		// IDrawable
		virtual void init(RendererResources* rendererResources, directx::DXCommandList& pCurrentCommandList, ResourceRegFunction& resRegFn) override;
		virtual void draw(directx::DXCommandList pCurrentCommandList, std::uint32_t currentBackBufferIndex) override;
		virtual void accept(IVisitor* visitor) override;

		// On matrix change
		virtual void onChange(directx::DXCommandList pCurrentCommandList, std::uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent) override;

		// On window resize
		virtual void onResize() override;

		// Is enabled?
		virtual bool isEnabled() const override;
		virtual void setEnabled(bool p_enabled) override;
		bool shouldClearAccumulation() const override;
		std::uint32_t getBufferUsage() const override;

		virtual void appendToPipeline(directx::RootSignatureManager* rootSignatureManager) override;
		virtual void appendToShaderTable(directx::ShadingTable* shadingTable) override;
		virtual void appendToDescHeapManager(directx::DescriptorHeap* descriptorHeap) override;


		const char* getName() const override;
	private:
		RendererResources* rendererResources;

		struct alignas(16) ConstBuff
		{
			std::uint32_t numSpeculars;
			float causticsRatio;
		} constBuffer;
		directx::DXResource constantBuffer;
	};
}