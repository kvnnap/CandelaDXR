#include "AccelerationStructure.h"
#include "Scene/Scene.h"

#include <string>
#include <cstdint>

using std::string;
using std::size_t;

using candela::directx::DXUtil;

using candela::renderer::RendererResources;
using candela::renderer::AccelerationStructure;

void AccelerationStructure::init(RendererResources* rendererResources, wrl::ComPtr<ID3D12GraphicsCommandList>& pCurrentCommandList)
{
	this->rendererResources = rendererResources;

	tlasTempBuffer.resize(rendererResources->numBackBuffers);

	auto scene = rendererResources->scene;

	const DXUtil::BottomLevelAccelerationData blasReferenceData
	{
		.vertexBuffer = rendererResources->sceneBuffer->GetGPUVirtualAddress(),
		.indexBuffer = rendererResources->sceneBuffer->GetGPUVirtualAddress() + scene->getIndicesOffset(),
		.vertexCount = static_cast<UINT>(scene->getVertices().size()),
		.indexCount = static_cast<UINT>(scene->getIndices().size()),
	};

	// Build bottom-layer - This incorporates all meshes - one BLAS per group
	// blasBuffers contains each mesh - so indexing is easy
	for (auto& meshIndex : scene->getMeshIndexedSpanData())
	{
		DXUtil::BottomLevelAccelerationData blasData = blasReferenceData;
		blasData.indexBuffer += static_cast<UINT>(meshIndex.Start) * sizeof(int);
		blasData.indexCount = static_cast<UINT>(meshIndex.Size);
		blasBuffers.push_back(DXUtil::createBottomLevelAS(rendererResources->pDevice, pCurrentCommandList, { blasData }, 3 * sizeof(float)));
	}

	// Instancing
	for (const auto& child : scene->getSceneGraph().getFlattenedMeshNodes())
	{
		auto& indexedSpan = scene->getMeshIndexedSpan(child.MeshId);
		auto& ref = tlasInstanceData.emplace_back(DXUtil::TopLevelAccelerationData {
			.instanceId = indexedSpan.Start,
			.blasBuffer = blasBuffers[child.MeshId]
		});
		XMStoreFloat3x4(&ref.transform, child.ComputedTransform);
	}

	// Build Top-Layer
	DXUtil::buildTopLevelAS(rendererResources->pDevice, pCurrentCommandList, tlasInstanceData, rendererResources->getTempResource(), false, tlasBuffers);
}

void candela::renderer::AccelerationStructure::onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent)
{
	if (changeEvent & static_cast<ChangeEvent_t>(ChangeEvent::Transformation))
		buildTlas(pCurrentCommandList, tlasTempBuffer[currentBackBufferIndex]);
}

void AccelerationStructure::buildTlas(wrl::ComPtr<ID3D12GraphicsCommandList>& commandList, wrl::ComPtr<ID3D12Resource>& tempResource)
{
	// Warning, we are assuming order - assuming scene graph structure isn't changing
	auto tlas = tlasInstanceData.begin();
	for (const auto* child : rendererResources->scene->getSceneGraph().getMeshNodes())
	{
		auto transform = child->getTransform();
		for (auto meshId : child->Meshes)
			XMStoreFloat3x4(&(tlas++)->transform, transform);
	}

	DXUtil::buildTopLevelAS(rendererResources->pDevice, commandList, tlasInstanceData, tempResource, true, tlasBuffers);
}

D3D12_GPU_VIRTUAL_ADDRESS AccelerationStructure::getTopLayerBufferAddress() const
{
	return tlasBuffers.pResult->GetGPUVirtualAddress();
}
