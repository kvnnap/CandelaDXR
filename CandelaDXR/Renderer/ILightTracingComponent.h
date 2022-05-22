#pragma once

#include "IDrawable.h"

#include "DirectX/RootSignatureManager.h"
#include "DirectX/ShadingTable.h"

namespace candela::renderer
{
	class ILightTracingComponent
		: public IDrawable
	{
	public:
		virtual void appendToPipeline(directx::RootSignatureManager *rootSignatureManager) = 0;
		virtual void appendToShaderTable(directx::ShadingTable *shadingTable) = 0;
		virtual void appendToDescHeapManager(directx::DescriptorHeap* descriptorHeap) = 0;
	};
}