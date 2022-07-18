#include "LTRasterGuidedShading.h"
#include "Mathematics/Utils.h"
#include "Mathematics/Constants.h"

#include "DirectX/DxUtil.h"

using std::uint32_t;

using candela::renderer::Camera;
using candela::directx::DXUtil;
using candela::sampler::ISampler;
using candela::mathematics::SamplePointOnTriangle;
using candela::mathematics::InterpolateVertices;
using candela::mathematics::GeneratePerpendicularVector;
using candela::mathematics::f1Definite;
using candela::mathematics::f2Definite;
using candela::mathematics::Vector2;
using candela::renderer::LTRasterGuidedShading;

LTRasterGuidedShading::LTRasterGuidedShading(ISampler* sampler, bool storePerLightCDF)
	: constBuffer(), sampler(sampler), rendererResources(),
	  cdfSize(512, 512), cumulativeDistributionTexture(), rasterShader(true),
	  rtaoShading({ "ltr_gPos", "ltr_gNorm", "ltr_gOut"}, { "ltr_cdf_mask" }),
	  storePerLightCDF(storePerLightCDF), regenerateCDFFlag()
{
}

void LTRasterGuidedShading::init(RendererResources* rRes, wrl::ComPtr<ID3D12GraphicsCommandList>& pCurrentCommandList, ResourceRegFunction& resRegFn)
{
	rendererResources = rRes;
	//lightCamera = std::make_unique<Camera>(*rRes->camera);
	lightCamera = std::make_unique<Camera>(DirectX::XMVECTOR{}, DirectX::XMVECTOR{0.f, 0.f, 1.f, 0.f}, 0.125f, 0.125f, 0.015625f, 1000.f);
	lightCamera->setAspectRatio(static_cast<float>(cdfSize.x) / cdfSize.y);

	rasterShader.setCamera(lightCamera.get());
	rasterShader.setGlobaResourcePrefix("ltr_");
	rasterShader.resize(&cdfSize);
	rasterShader.init(rRes, pCurrentCommandList, resRegFn);
	rasterShader.setComputeRadiance(false); // Set this after init!

	rtaoShading.init(rRes, pCurrentCommandList, resRegFn);

	cdfs.resize(storePerLightCDF ? rendererResources->scene->getLights().size() : 1);
	resources.resize(cdfs.size());
	for (std::size_t i = 0; i < cdfs.size(); ++i)
		cdfs[i] = resources[i] = & rendererResources->resourceManager->createResource(
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
			cdfSize.x, cdfSize.y,
			DXGI_FORMAT_R32_FLOAT, false, "cdf_" + std::to_string(i));

	auto posTex = rendererResources->resourceManager->getNamedResource("ltr_gPos");

	resources.push_back(posTex);
	const uint32_t inputIndex = static_cast<uint32_t>(resources.size() - 1);
	const uint32_t numOutputs = static_cast<uint32_t>(cdfs.size());

	distanceComputerShader.init(rRes, pCurrentCommandList, &resources, numOutputs);
	distanceComputerShader.setInputTexture(inputIndex);

	// multiple inputs, one output
	guassianComputerShader.init(rRes, pCurrentCommandList, &resources, numOutputs);
	guassianComputerShader.setInputTexture(inputIndex);

	prefixSumComputeShader.init(rRes, pCurrentCommandList, &resources, numOutputs);
	prefixSumComputeShader.setInputTexture(inputIndex); // dummy

	normalisationComputeShader.init(rRes, pCurrentCommandList, &resources, numOutputs);
	normalisationComputeShader.setInputTexture(inputIndex); // dummy

	normalisationPass2ComputeShader.init(rRes, pCurrentCommandList, &resources, numOutputs);
	normalisationPass2ComputeShader.setInputTexture(inputIndex); // dummy

	regenerateCDFs(pCurrentCommandList, 0);

	// Constant buffer
	const auto sensorDim = lightCamera->getNearPlaneDimensions();
	float x = sensorDim.m128_f32[0] * 0.5f;
	float y = sensorDim.m128_f32[1] * 0.5f;
	constBuffer.lightCamPdf = f1Definite(-x, x, -y, y, sensorDim.m128_f32[2]) * candela::mathematics::constants::OneOverPi;
	float hemiSphericalCoverAreaPercent = f2Definite(-x, x, -y, y, sensorDim.m128_f32[2]) * candela::mathematics::constants::OneOverTwoPi;
	constBuffer.plane = DirectX::XMVectorScale(sensorDim, 1 / sensorDim.m128_f32[2]);
	constBuffer.lightCamDim = cdfSize;
	constantBuffer = DXUtil::uploadDataToDefaultHeap(rendererResources->pDevice, pCurrentCommandList, rendererResources->getTempResource(), &constBuffer, sizeof(constBuffer), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	constantBuffer->SetName(L"LT Raster Guided Constant Buffer");
}

void LTRasterGuidedShading::setFilterSize(std::uint32_t filterSize)
{
	guassianComputerShader.setFiltersize(filterSize);
	regenerateCDFFlag = true;
}

uint32_t LTRasterGuidedShading::getFilterSize() const
{
	return guassianComputerShader.getFiltersize();
}

void LTRasterGuidedShading::setDistanceMetricMode(std::uint32_t mode)
{
	distanceComputerShader.setMode(mode);
}

uint32_t LTRasterGuidedShading::getDistanceMetricMode() const
{
	return distanceComputerShader.getMode();
}

void LTRasterGuidedShading::generateCDF(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, uint32_t currentBackBufferIndex, uint32_t lightIndex)
{
	// Logic for choosing light and setting up camera from light
	auto scene = rendererResources->scene;
	auto& indices = scene->getIndices();
	auto& vertices = scene->getVertices();
	auto& normals = scene->getNormals();

	// Choose light
	auto& lights = scene->getLights();
	auto& light = lights[lightIndex]; // lights[0]
	auto lightIndexId = light.PrimitiveId * 3;
	auto i0 = indices[lightIndexId + 0];
	auto i1 = indices[lightIndexId + 1];
	auto i2 = indices[lightIndexId + 2];

	Vector2 uv;//  = SamplePointOnTriangle(*sampler);
	uv.x = uv.y = 1.f / 3.f;

	DirectX::XMVECTOR pos = InterpolateVertices(uv,
		DirectX::XMLoadFloat3(&vertices[i0]),
		DirectX::XMLoadFloat3(&vertices[i1]),
		DirectX::XMLoadFloat3(&vertices[i2]));
	pos.m128_f32[3] = 1.f;

	DirectX::XMVECTOR nor = InterpolateVertices(uv,
		DirectX::XMLoadFloat3(&normals[i0]),
		DirectX::XMLoadFloat3(&normals[i1]),
		DirectX::XMLoadFloat3(&normals[i2]));

	auto& sceneNode = scene->getSceneGraph();
	const auto& lightTransform = sceneNode.Children[light.InstanceIndex].Transform;
	const auto normalTransform = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, lightTransform));

	// Apply necessary transforms
	pos = DirectX::XMVector4Transform(pos, lightTransform);
	nor = DirectX::XMVector4Transform(nor, normalTransform);
	auto up = GeneratePerpendicularVector(nor);

	// Alter Camera
	lightCamera->lookTo(pos, nor, up);
	distanceComputerShader.distConstBuffer.position = lightCamera->getPosition();
	distanceComputerShader.distConstBuffer.plane = lightCamera->getNearPlaneDimensions();
	rtaoShading.setCameraPosition(rendererResources->camera->getPosition());

	// Send user camera
	distanceComputerShader.distConstBuffer.camPosition = rendererResources->camera->getPosition();
	distanceComputerShader.distConstBuffer.camUnitDir = rendererResources->camera->getDirection();

	// Generate CDF
	rasterShader.draw(pCurrentCommandList, currentBackBufferIndex);
	if(getDistanceMetricMode() == 2)
		rtaoShading.draw(pCurrentCommandList, currentBackBufferIndex);
	if (cumulativeDistributionTexture->getState() != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		cumulativeDistributionTexture->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	distanceComputerShader.compute(pCurrentCommandList);
	guassianComputerShader.compute(pCurrentCommandList);
	prefixSumComputeShader.compute(pCurrentCommandList);
	normalisationComputeShader.compute(pCurrentCommandList);
	normalisationPass2ComputeShader.compute(pCurrentCommandList);
	cumulativeDistributionTexture->transistionBarrier(pCurrentCommandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void LTRasterGuidedShading::regenerateCDFs(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, uint32_t currentBackBufferIndex)
{
	for (uint32_t i = 0; i < cdfs.size(); ++i)
	{
		cumulativeDistributionTexture = cdfs[i];
		distanceComputerShader.setOutputTexture(i);
		guassianComputerShader.setOutputTexture(i);
		prefixSumComputeShader.setOutputTexture(i);
		normalisationComputeShader.setOutputTexture(i);
		normalisationPass2ComputeShader.setOutputTexture(i);
		generateCDF(pCurrentCommandList, currentBackBufferIndex, i);
	}
}

void LTRasterGuidedShading::draw(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, uint32_t currentBackBufferIndex)
{
	constBuffer.lightIndex = storePerLightCDF ? UINT_MAX : static_cast<uint32_t>(sampler->chooseInRange(0, rendererResources->scene->getLights().size() - 1));
	DXUtil::updateDataInDefaultHeap(rendererResources->pDevice, pCurrentCommandList, constantBuffer, rendererResources->getTempResource(),
		&constBuffer, sizeof(constBuffer), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	if (storePerLightCDF)
	{
		if (regenerateCDFFlag)
		{
			regenerateCDFs(pCurrentCommandList, currentBackBufferIndex);
			regenerateCDFFlag = false;
		}
	}
	else
	{
		generateCDF(pCurrentCommandList, currentBackBufferIndex, constBuffer.lightIndex);
	}
}

void LTRasterGuidedShading::accept(IVisitor* visitor)
{
	visitor->visit(this);
}

void LTRasterGuidedShading::onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent)
{
	rasterShader.onChange(pCurrentCommandList, currentBackBufferIndex, changeEvent);
	if (!storePerLightCDF)
		return;
	if (changeEvent)// & (static_cast<ChangeEvent_t>(ChangeEvent::Transformation) | static_cast<ChangeEvent_t>(ChangeEvent::SceneChange)))
		regenerateCDFFlag = true;
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
	auto numLights = static_cast<uint32_t>(rendererResources->scene->getLights().size());
	rootSignatureManager->addDescriptorRange("BVH", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, static_cast<uint32_t>(cdfs.size()), 0u, 1u)); // gCDF
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

	auto entryNumber = descriptorHeap->getSetResourcesSize() - cdfs.size();
	for (uint32_t i = 0; i < cdfs.size(); ++i)
		descriptorHeap->setSRV(entryNumber++, srvDesc, rendererResources->pDevice, *cdfs[i]);
}
