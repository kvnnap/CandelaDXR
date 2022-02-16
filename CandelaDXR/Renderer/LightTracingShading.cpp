#include "LightTracingShading.h"

#include <DirectXMath.h>

#include "DirectX/DxUtil.h"

using std::uint32_t;
using std::vector;
using std::string;
using std::unordered_map;

using DirectX::XMFLOAT3X4;

using candela::directx::DXUtil;

using candela::renderer::LightTracingShading;
using candela::renderer::RendererResources;

LightTracingShading::LightTracingShading()
	: rendererResources(), constBuffer()
{
}

void LightTracingShading::init(RendererResources* rRes)
{
	rendererResources = rRes;

	auto commandList = rRes->commandQueue->getCommandList();
	//vector<D3D12_GPU_VIRTUAL_ADDRESS>& vertexBuffers;
	auto& scene = *rRes->scene;

	// Build bottom-layer - This incorporates all meshes
	unordered_map<string, size_t> bufferMap;
	for (auto &item : scene.getMeshIndexedSpanDataMap())
	{
		auto mis = &item.second;
		bufferMap[item.first] = blasBuffers.size();
		blasBuffers.push_back(
			DXUtil::createBottomLevelAS(
				rRes->pDevice, commandList, 
				{ rRes->sceneBuffer->GetGPUVirtualAddress() + mis->Start * 3 * sizeof(float) },
				{ static_cast<uint32_t>(mis->Size) / 3 }, 3 * sizeof(float)
			)
		);
	}

	vector<DXUtil::TopLevelAccelerationData> instanceData;
	for (auto child : scene.getSceneGraph().Children)
	{
		auto &indexedSpan = scene.getMeshIndexedSpan(child.GroupName);
		auto &ref = instanceData.emplace_back(DXUtil::TopLevelAccelerationData {
			.instanceId = indexedSpan.Start,
			.blasBuffer = blasBuffers[bufferMap.at(child.GroupName)]
		});
		XMStoreFloat3x4(&ref.transform, child.Transform);
	}

	// Build Top-Layer
	wrl::ComPtr<ID3D12Resource> tlasTempBuffer;
	DXUtil::buildTopLevelAS(rRes->pDevice, commandList, instanceData, tlasTempBuffer, false, tlasBuffers);
}

void LightTracingShading::draw(wrl::ComPtr<ID3D12GraphicsCommandList6> currentCommandList, uint32_t currentBackBufferIndex)
{
}
