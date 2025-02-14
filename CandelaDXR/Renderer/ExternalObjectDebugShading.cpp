#include <d3dcompiler.h>
#include "DirectX/DxUtil.h"

#include "Exception/WindowException.h"


#include "ExternalObjectDebugShading.h"

#include "Mathematics/Plane.h"

using std::uint32_t;
using std::make_shared;
using Microsoft::WRL::ComPtr;

using candela::renderer::ExternalObjectDebugShading;
using candela::mathematics::Vector3;
using candela::directx::DXUtil;
using candela::directx::RootSignatureManager;
using candela::mathematics::Plane;

ExternalObjectDebugShading::ExternalObjectDebugShading()
	: rendererResources(), constBuffer(), bufferView{}, scissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX)), viewport{}, dsvDescriptorSize{}, pDepthBuffer{}, needsUpdate{}, displaySceneAabb{}
{
	setName("EODebug");
}

void ExternalObjectDebugShading::init(RendererResources* rRes, wrl::ComPtr<ID3D12GraphicsCommandList>& pCurrentCommandList, ResourceRegFunction& resRegFn)
{
	this->rendererResources = rRes;
	// Handle result used for errors
	HRESULT hr;

	// Load shaders
	wrl::ComPtr<ID3DBlob> pVertexShaderBlob = DXUtil::LoadShaderResource("./Shaders/ExternalObjectVS.cso");
	wrl::ComPtr<ID3DBlob> pPixelShaderBlob = DXUtil::LoadShaderResource("./Shaders/ExternalObjectPS.cso");

	// Input Assembler config
	D3D12_INPUT_ELEMENT_DESC ied[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	//bufferView = D3D12_VERTEX_BUFFER_VIEW{
	//	.BufferLocation = rRes->sceneBuffer->GetGPUVirtualAddress() + rRes->scene->getVerticesOffset(),
	//	.SizeInBytes = static_cast<std::uint32_t>(rRes->scene->getVerticesSizeBytes()),
	//	.StrideInBytes = sizeof(Vector3)
	//};

	// And for depth stencil view
	pDepthDescriptorHeap = DXUtil::createDescriptorHeap(rRes->pDevice, 1u, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	pDepthDescriptorHeap->SetName(L"External Object Depth Descriptor Heap");
	dsvDescriptorSize = rRes->pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	// Allow input layout and deny unnecessary access to certain pipeline stages.
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		| D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
		| D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
		| D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
		//| D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS
		;

	CD3DX12_ROOT_PARAMETER1 param{};
	rootSignatureManager = make_shared<RootSignatureManager>();
	param.InitAsConstantBufferView(0u); rootSignatureManager->setParameter("CBV", param);
	rootSignatureManager->addParametersToRootSignature("RasterRootSignature", { "CBV" });
	rootSignatureManager->setSamplerForRootSignature("RasterRootSignature", DXUtil::getDefaultSamplerDesc());
	rootSignature = rootSignatureManager->generateRootSignature("RasterRootSignature", rendererResources->pDevice, rootSignatureFlags);

	// Setup pipeline - TYPE info is contained within each property in the struct!
	struct PipelineStateStream
	{
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS VS;
		CD3DX12_PIPELINE_STATE_STREAM_PS PS;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
	} pipelineStateStream;

	D3D12_RT_FORMAT_ARRAY rtvFormats{};
	rtvFormats.NumRenderTargets = 1u;
	rtvFormats.RTFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;

	auto rasterDesc = CD3DX12_RASTERIZER_DESC(CD3DX12_DEFAULT());
	rasterDesc.CullMode = D3D12_CULL_MODE_NONE;

	pipelineStateStream.pRootSignature = rootSignature.Get();
	pipelineStateStream.InputLayout = { ied, static_cast<std::uint32_t>(std::size(ied)) };
	pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(pVertexShaderBlob.Get());
	pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(pPixelShaderBlob.Get());
	pipelineStateStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	pipelineStateStream.RTVFormats = rtvFormats;
	pipelineStateStream.Rasterizer = rasterDesc;

	D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
		sizeof(PipelineStateStream), &pipelineStateStream
	};
	ComPtr<ID3D12Device2> pDevice2;
	GFXTHROWIFFAILED(rRes->pDevice.As(&pDevice2));
	GFXTHROWIFFAILED(pDevice2->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&pipelineState)));

	// Init const buffer
	//constBuffer.numLights = static_cast<uint32_t>(rendererResources->scene->getLights().size());
	constantBuffer = DXUtil::uploadDataToDefaultHeap(
		rRes->pDevice,
		pCurrentCommandList,
		rendererResources->getTempResource(),
		&constBuffer,
		sizeof(constBuffer),
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	constantBuffer->SetName(L"Constant Buffer");

	onResize();
	updateBuffer(pCurrentCommandList);
}

void ExternalObjectDebugShading::draw(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, std::uint32_t currentBackBufferIndex)
{
	if (needsUpdate)
		updateBuffer(pCurrentCommandList);

	if (vertices.empty())
		return;	

	// Clear Depth
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvDescriptorHandle(pDepthDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), 0, dsvDescriptorSize);
	pCurrentCommandList->ClearDepthStencilView(dsvDescriptorHandle, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

	pCurrentCommandList->SetGraphicsRootSignature(rootSignature.Get());
	pCurrentCommandList->SetPipelineState(pipelineState.Get());

	pCurrentCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pCurrentCommandList->IASetVertexBuffers(0u, 1u, &bufferView);

	pCurrentCommandList->RSSetScissorRects(1u, &scissorRect);
	pCurrentCommandList->RSSetViewports(1u, &viewport);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandles[1]{};
	const auto incrementSize = rendererResources->pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	rtvDescriptorHandles[0] = CD3DX12_CPU_DESCRIPTOR_HANDLE(rendererResources->pRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		rendererResources->numBackBuffers, incrementSize);
	pCurrentCommandList->OMSetRenderTargets(1u, &rtvDescriptorHandles[0], FALSE, &dsvDescriptorHandle);

	// Update
	constBuffer.ViewPerspective = rendererResources->camera->getViewPerspectiveMatrixColMajor();
	DXUtil::updateDataInDefaultHeap(
		rendererResources->pDevice,
		pCurrentCommandList,
		constantBuffer,
		rendererResources->getTempResource(),
		&constBuffer,
		sizeof(constBuffer),
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	pCurrentCommandList->SetGraphicsRootConstantBufferView(0u, constantBuffer->GetGPUVirtualAddress()); // Const buff (includes Cam)
	pCurrentCommandList->DrawInstanced(static_cast<UINT>(vertices.size()), 1u, 0u, 0u);
}

void ExternalObjectDebugShading::onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList,
	std::uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent)
{
	if (changeEvent & static_cast<ChangeEvent_t>(ChangeEvent::Transformation) | changeEvent & static_cast<ChangeEvent_t>(ChangeEvent::SceneChange))
		updateBuffer(pCurrentCommandList);
}

void ExternalObjectDebugShading::onResize()
{
	viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, 
		static_cast<float>(rendererResources->winDimensions.x), 
		static_cast<float>(rendererResources->winDimensions.y));
	auto temp = DXUtil::createDepthStencilView(rendererResources->pDevice, pDepthDescriptorHeap, 
		rendererResources->winDimensions.x, rendererResources->winDimensions.y, 1u)[0];
	if (!pDepthBuffer)
		pDepthBuffer = &rendererResources->resourceManager->addExistingResource(temp, D3D12_RESOURCE_STATE_DEPTH_WRITE, "ext_gDepth");
	pDepthBuffer->setResource(temp, D3D12_RESOURCE_STATE_DEPTH_WRITE);
}

void ExternalObjectDebugShading::accept(IVisitor* visitor)
{
	visitor->visit(this);
}

uint32_t ExternalObjectDebugShading::getBufferUsage() const
{
	return BufferUsage::Radiance;
}

void ExternalObjectDebugShading::setEnabled(bool p_enabled)
{
	Drawable::setEnabled(p_enabled);
	if (!isEnabled())
		return;
	needsUpdate = true;
	//auto cq = rendererResources->commandQueue;
	//cq->flush();
	//auto list = cq->getCommandList();
	//updateBuffer(list);
	//cq->executeCommandList(list);
}

void ExternalObjectDebugShading::setVertices(std::vector<mathematics::Vector3>&& verts)
{
}

void ExternalObjectDebugShading::setDisplaySceneAabb(bool p_disp)
{
	displaySceneAabb = p_disp;
	needsUpdate = true;
}

bool ExternalObjectDebugShading::getDisplaySceneAabb() const
{
	return displaySceneAabb;
}

void ExternalObjectDebugShading::updateBuffer(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList)
{
	if (!isEnabled())
		return;

	using namespace DirectX;

	vertices.clear();

	const auto aabb = rendererResources->scene->getSceneAABB();
	if (displaySceneAabb)
		appendAabb(aabb);

	for (const auto& myLight : rendererResources->processedExternalLights)
	{
		if (myLight.Type == LT_POINT || myLight.Type == LT_SPOT)
		{
			mathematics::AABB pointAABB;
			constexpr float deltaP = 0.01f;
			pointAABB.contain(myLight.Position);
			pointAABB.contain(myLight.Position - mathematics::Vector{ deltaP, deltaP, deltaP, 0.f });
			pointAABB.contain(myLight.Position + mathematics::Vector{ deltaP, deltaP, deltaP, 0.f });
			appendAabb(pointAABB);
		}
		else if (myLight.Type == LT_DIRECTIONAL)
		{
			auto plane = Plane(myLight.Position, myLight.Direction);
			Vector3 vec3;
			mathematics::Vector uvCoords = XMVectorSet(0, myLight.AreaDimensions.y, 0, 0);
			auto tempVec = myLight.Position + uvCoords.m128_f32[0] * myLight.Right + uvCoords.m128_f32[1] * myLight.Up;
			XMStoreFloat3(&vec3, tempVec);
			vertices.push_back(vec3);

			uvCoords = XMVectorSet(0, 0, 0, 0);
			tempVec = myLight.Position + uvCoords.m128_f32[0] * myLight.Right + uvCoords.m128_f32[1] * myLight.Up;
			XMStoreFloat3(&vec3, tempVec);
			vertices.push_back(vec3);

			uvCoords = XMVectorSet(myLight.AreaDimensions.x, myLight.AreaDimensions.y, 0, 0);
			tempVec = myLight.Position + uvCoords.m128_f32[0] * myLight.Right + uvCoords.m128_f32[1] * myLight.Up;
			XMStoreFloat3(&vec3, tempVec);
			vertices.push_back(vec3);

			vertices.push_back(vertices.back());
			vertices.push_back(vertices[vertices.size() - 3]);

			uvCoords = XMVectorSet(myLight.AreaDimensions.x, 0, 0, 0);
			tempVec = myLight.Position + uvCoords.m128_f32[0] * myLight.Right + uvCoords.m128_f32[1] * myLight.Up;
			XMStoreFloat3(&vec3, tempVec);
			vertices.push_back(vec3);
		}
	}

	needsUpdate = false;
	if (vertices.empty())
		return;

	// need to flush when replacing buffer (or keep old one around until it is not used anymore)
	rendererResources->commandQueue->flush();
	
	vertexBuffer = std::make_unique<directx::Resource>(directx::Resource::createCommittedResource(
		rendererResources->pDevice, vertices.size() * sizeof(Vector3), D3D12_RESOURCE_STATE_COMMON));
	vertexBuffer->write(pCurrentCommandList, rendererResources->getTempResource(), vertices.data());
	vertexBuffer->setName("External Object Vertex Buffer");
	ID3D12Resource* pt = *vertexBuffer;

	// Update buffer view?
	bufferView = D3D12_VERTEX_BUFFER_VIEW{
		.BufferLocation = pt->GetGPUVirtualAddress(),
		.SizeInBytes = static_cast<std::uint32_t>(vertices.size() * sizeof(Vector3)),
		.StrideInBytes = sizeof(Vector3)
	};
}

void ExternalObjectDebugShading::appendAabb(const mathematics::AABB& aabb)
{
	std::vector<Vector3> v(8);
	for (int i = 0; i < 8; ++i)
		DirectX::XMStoreFloat3(&v[i], aabb.getCornerPoint(i));

	// Front
	vertices.push_back(v[0]);
	vertices.push_back(v[1]);
	vertices.push_back(v[2]);

	vertices.push_back(v[2]);
	vertices.push_back(v[1]);
	vertices.push_back(v[3]);

	// Left
	vertices.push_back(v[4]);
	vertices.push_back(v[0]);
	vertices.push_back(v[6]);

	vertices.push_back(v[6]);
	vertices.push_back(v[0]);
	vertices.push_back(v[2]);

	// Back
	vertices.push_back(v[5]);
	vertices.push_back(v[4]);
	vertices.push_back(v[7]);

	vertices.push_back(v[7]);
	vertices.push_back(v[4]);
	vertices.push_back(v[6]);

	// Right
	vertices.push_back(v[1]);
	vertices.push_back(v[5]);
	vertices.push_back(v[3]);

	vertices.push_back(v[3]);
	vertices.push_back(v[5]);
	vertices.push_back(v[7]);

	// Top
	vertices.push_back(v[2]);
	vertices.push_back(v[3]);
	vertices.push_back(v[6]);

	vertices.push_back(v[6]);
	vertices.push_back(v[3]);
	vertices.push_back(v[7]);

	// Bottom
	vertices.push_back(v[4]);
	vertices.push_back(v[5]);
	vertices.push_back(v[0]);

	vertices.push_back(v[0]);
	vertices.push_back(v[5]);
	vertices.push_back(v[1]);
}

