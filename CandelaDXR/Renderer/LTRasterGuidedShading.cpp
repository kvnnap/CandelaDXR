#include "LTRasterGuidedShading.h"
#include "Mathematics/Utils.h"

#include "DirectX/DxUtil.h"

using std::uint32_t;

using candela::renderer::Camera;
using candela::directx::DXUtil;
using candela::sampler::ISampler;
using candela::mathematics::SamplePointOnTriangle;
using candela::mathematics::InterpolateVertices;
using candela::mathematics::GeneratePerpendicularVector;
using candela::renderer::LTRasterGuidedShading;

LTRasterGuidedShading::LTRasterGuidedShading(ISampler* sampler)
	: sampler(sampler), constBuffer(), rendererResources(), cdfSize(512, 512), cumulativeDistributionTexture(), rasterShader(true)
{
}

void LTRasterGuidedShading::init(RendererResources* rRes, wrl::ComPtr<ID3D12GraphicsCommandList>& pCurrentCommandList, ResourceRegFunction& resRegFn)
{
	rendererResources = rRes;
	lightCamera = std::make_unique<Camera>(*rRes->camera);

	rasterShader.setCamera(lightCamera.get());
	rasterShader.setGlobaResourcePrefix("ltr_");
	rasterShader.resize(&cdfSize);
	rasterShader.init(rRes, pCurrentCommandList, resRegFn);
	rasterShader.setComputeRadiance(false); // Set this after init!
	cumulativeDistributionTexture = &rendererResources->resourceManager->createResource(
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		cdfSize.x, cdfSize.y,
		DXGI_FORMAT_R32_FLOAT, false, "cdf");

	distanceComputerShader.setAdditionalConstantBuffer(&rRes->camera->getPosition(), sizeof(float) * 3);
	distanceComputerShader.init(rRes, pCurrentCommandList);
	distanceComputerShader.setInputTexture("ltr_gPos");
	distanceComputerShader.setOutputTexture(cumulativeDistributionTexture);

	prefixSumComputeShader.init(rRes, pCurrentCommandList);
	prefixSumComputeShader.setInputTexture("ltr_gPos");
	prefixSumComputeShader.setOutputTexture(cumulativeDistributionTexture);

	normalisationComputeShader.init(rRes, pCurrentCommandList);
	normalisationComputeShader.setInputTexture("ltr_gPos");
	normalisationComputeShader.setOutputTexture(cumulativeDistributionTexture);

	normalisationPass2ComputeShader.init(rRes, pCurrentCommandList);
	normalisationPass2ComputeShader.setInputTexture("ltr_gPos");
	normalisationPass2ComputeShader.setOutputTexture(cumulativeDistributionTexture);

	// Constant buffer
	constantBuffer = DXUtil::uploadDataToDefaultHeap(rendererResources->pDevice, pCurrentCommandList, rendererResources->getTempResource(), &constBuffer, sizeof(constBuffer), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	constantBuffer->SetName(L"LT Raster Guided Constant Buffer");
}

void LTRasterGuidedShading::draw(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, uint32_t currentBackBufferIndex)
{
	// Logic for choosing light and setting up camera from light
	auto scene = rendererResources->scene;

	// Choose light
	auto& lights = scene->getLights();
	auto& light = lights[constBuffer.lightIndex = static_cast<uint32_t>(sampler->chooseInRange(0, lights.size() - 1))]; // lights[0]
	auto lightIndexId = light.PrimitiveId * 3;
	auto i0 = scene->getIndices()[lightIndexId + 0];
	auto i1 = scene->getIndices()[lightIndexId + 1];
	auto i2 = scene->getIndices()[lightIndexId + 2];

	auto uv = SamplePointOnTriangle(*sampler);
	//uv.x = uv.y = 1.f / 3.f;
	
	DirectX::XMVECTOR pos = InterpolateVertices(uv,
		DirectX::XMLoadFloat3(&scene->getVertices()[i0]),
		DirectX::XMLoadFloat3(&scene->getVertices()[i1]),
		DirectX::XMLoadFloat3(&scene->getVertices()[i2]));
	pos.m128_f32[3] = 1.f;

	DirectX::XMVECTOR nor = InterpolateVertices(uv,
		DirectX::XMLoadFloat3(&scene->getNormals()[i0]),
		DirectX::XMLoadFloat3(&scene->getNormals()[i1]),
		DirectX::XMLoadFloat3(&scene->getNormals()[i2]));

	auto& sceneNode = scene->getSceneGraph();
	const auto& lightTransform = sceneNode.Children[light.InstanceIndex].Transform;
	const auto normalTransform = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, lightTransform));
	
	// Apply necessary transforms
	pos = DirectX::XMVector4Transform(pos, lightTransform);
	nor = DirectX::XMVector4Transform(nor, normalTransform);
	auto up = GeneratePerpendicularVector(nor);

	// Alter Camera
	lightCamera->lookTo(pos, nor, up);

	// Update Constant Buffer
	constBuffer.w = DirectX::XMVector3Normalize(lightCamera->getDirection());
	constBuffer.u = DirectX::XMVectorNegate(DirectX::XMVector3Normalize(DirectX::XMVector3Cross(lightCamera->getUp(), lightCamera->getDirection())));
	constBuffer.v = DirectX::XMVectorNegate(DirectX::XMVector3Normalize(DirectX::XMVector3Cross(constBuffer.w, constBuffer.u)));
	constBuffer.position = lightCamera->getPosition();
	constBuffer.direction = lightCamera->getDirection();
	constBuffer.plane = lightCamera->getNearPlaneDimensions();
	constBuffer.lightCamDim = cdfSize;

	DXUtil::updateDataInDefaultHeap(rendererResources->pDevice, pCurrentCommandList, constantBuffer, rendererResources->getTempResource(),
		&constBuffer, sizeof(constBuffer), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	// Generate CDF
	rasterShader.draw(pCurrentCommandList, currentBackBufferIndex);
	if (cumulativeDistributionTexture->getState() != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		cumulativeDistributionTexture->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	distanceComputerShader.compute(pCurrentCommandList);
	prefixSumComputeShader.compute(pCurrentCommandList);
	normalisationComputeShader.compute(pCurrentCommandList);
	normalisationPass2ComputeShader.compute(pCurrentCommandList);
	cumulativeDistributionTexture->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	// Anything else?
}

void LTRasterGuidedShading::accept(IVisitor* visitor)
{
	visitor->visit(this);
}

void LTRasterGuidedShading::onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent)
{
	rasterShader.onChange(pCurrentCommandList, currentBackBufferIndex, changeEvent);
}

void LTRasterGuidedShading::onResize()
{
	rasterShader.onResize();
}

bool LTRasterGuidedShading::isEnabled() const
{
	return false;
}

void LTRasterGuidedShading::setEnabled(bool p_enabled)
{
}

void LTRasterGuidedShading::appendToPipeline(directx::RootSignatureManager* rootSignatureManager)
{
	CD3DX12_ROOT_PARAMETER1 param;
	param.InitAsConstantBufferView(0u, 1u); rootSignatureManager->setParameter("SecondConstBuff", param);
	rootSignatureManager->addParameterToRootSignature("RayGenRootSignature", "SecondConstBuff");
	rootSignatureManager->addDescriptorRange("BVH", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1u, 0u, 1u)); // gCDF
	rootSignatureManager->setDescriptorTableParameter("BVHDescTable", "BVH");
}

void LTRasterGuidedShading::appendToShaderTable(directx::ShadingTable* shadingTable)
{
	shadingTable->setInputForViewParameter(L"rayGen", "SecondConstBuff", constantBuffer);
}

void LTRasterGuidedShading::appendToDescHeapManager(directx::DescriptorHeap* descriptorHeap)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1u;

	descriptorHeap->setSRV(descriptorHeap->getSetResourcesSize() - 1, srvDesc, rendererResources->pDevice, *cumulativeDistributionTexture);
}
