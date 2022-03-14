#include <d3dcompiler.h>
#include <vector>

#include "RasterShading.h"
#include "DirectX/d3dx12.h"
#include "DirectX/DxUtil.h"

#include "Mathematics/Types.h"

#include "Exception/WindowException.h"

using candela::mathematics::Vector2;
using candela::mathematics::Vector3;
using candela::mathematics::UVector2;
using candela::renderer::RasterShading;
using candela::renderer::Camera;
using candela::renderer::ChangeEvent_t;
using candela::directx::DXUtil;
using candela::directx::RootSignatureManager;
using candela::directx::DescriptorHeap;
using DirectX::XMMATRIX;
using std::make_shared;
using std::uint32_t;
using std::vector;
using Microsoft::WRL::ComPtr;

RasterShading::RasterShading()
	: constBuffer{}, scissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
{
}

void candela::renderer::RasterShading::init(RendererResources* rRes)
{
	this->rendererResources = rRes;

	constantTempBuffer.resize(rRes->numBackBuffers);

	// Handle result used for errors
	HRESULT hr;

	viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(rRes->winDimensions.x), static_cast<float>(rRes->winDimensions.y));

	// Load shaders
	wrl::ComPtr<ID3DBlob> pVertexShaderBlob;
	wrl::ComPtr<ID3DBlob> pPixelShaderBlob;
	GFXTHROWIFFAILED(D3DReadFileToBlob(L"./Shaders/VertexShader.cso", &pVertexShaderBlob));
	GFXTHROWIFFAILED(D3DReadFileToBlob(L"./Shaders/PixelShader.cso", &pPixelShaderBlob));

	// Input Assembler config
	D3D12_INPUT_ELEMENT_DESC ied[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXUV", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, //scene.getVertices().size() * sizeof(Vector3)
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 2, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 } // scene.getVertices().size() * sizeof(Vector3) + scene.getTextureCoords().size() * sizeof(Vector2)
	};
	bufferViews[0] = D3D12_VERTEX_BUFFER_VIEW{
		.BufferLocation = rRes->sceneBuffer->GetGPUVirtualAddress() + rRes->scene->getVerticesOffset(),
		.SizeInBytes = static_cast<std::uint32_t>(rRes->scene->getVerticesSizeBytes()),
		.StrideInBytes = sizeof(Vector3)
	};
	bufferViews[1] = D3D12_VERTEX_BUFFER_VIEW{
		.BufferLocation = rRes->sceneBuffer->GetGPUVirtualAddress() + rRes->scene->getTextureCoordsOffset(),
		.SizeInBytes = static_cast<std::uint32_t>(rRes->scene->getTextureCoordsSizeBytes()),
		.StrideInBytes = sizeof(Vector2)
	};
	bufferViews[2] = D3D12_VERTEX_BUFFER_VIEW{
		.BufferLocation = rRes->sceneBuffer->GetGPUVirtualAddress() + rRes->scene->getNormalsOffset(),
		.SizeInBytes = static_cast<std::uint32_t>(rRes->scene->getNormalsSizeBytes()),
		.StrideInBytes = sizeof(Vector3)
	};
	indexView.BufferLocation = rRes->sceneBuffer->GetGPUVirtualAddress() + rRes->scene->getIndicesOffset();
	indexView.SizeInBytes = static_cast<UINT>(rRes->scene->getIndicesSizeBytes());
	indexView.Format = DXGI_FORMAT_R32_UINT;

	// And for depth stencil view
	pDepthDescriptorHeap = DXUtil::createDescriptorHeap(rRes->pDevice, rRes->numBackBuffers, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	pDepthDescriptorHeap->SetName(L"Depth Descriptor Heap");
	dsvDescriptorSize = rRes->pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	pDepthBuffers = DXUtil::createDepthStencilView(rRes->pDevice, pDepthDescriptorHeap, rRes->winDimensions.x, rRes->winDimensions.y, rRes->numBackBuffers);

	// Allow input layout and deny unnecessary access to certain pipeline stages.
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		| D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
		| D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
		| D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
		//| D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS
		;

	D3D12_STATIC_SAMPLER_DESC sampler = {};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	sampler.MinLOD = 0.0f;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 0;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	CD3DX12_ROOT_PARAMETER1 param;
	rootSignatureManager = make_shared<RootSignatureManager>();
	param.InitAsShaderResourceView(0u); rootSignatureManager->setParameter("Material", param);
	param.InitAsShaderResourceView(1u); rootSignatureManager->setParameter("Face", param);
	param.InitAsShaderResourceView(2u); rootSignatureManager->setParameter("Light", param);
	param.InitAsShaderResourceView(3u); rootSignatureManager->setParameter("Vertices", param);
	param.InitAsShaderResourceView(4u); rootSignatureManager->setParameter("TexUV", param);
	param.InitAsShaderResourceView(5u); rootSignatureManager->setParameter("Normals", param);
	param.InitAsShaderResourceView(6u); rootSignatureManager->setParameter("Indices", param);
	param.InitAsShaderResourceView(7u); rootSignatureManager->setParameter("Matrices", param);
	rootSignatureManager->addDescriptorRange("Textures", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, static_cast<uint32_t>(rendererResources->textures.size()), 8u)); // gIrrToRad
	rootSignatureManager->setDescriptorTableParameter("TexturesDescTable", "Textures");
	param.InitAsConstantBufferView(0u); rootSignatureManager->setParameter("CBV", param);
	param.InitAsConstants(2u, 1u); rootSignatureManager->setParameter("Constants", param);
	rootSignatureManager->addParametersToRootSignature("RasterRootSignature", { "CBV", "Constants", "Material", "Face", "Light", "Vertices", "TexUV", "Normals", "Indices", "Matrices", "TexturesDescTable"});
	rootSignatureManager->setSamplerForRootSignature("RasterRootSignature", sampler);
	rootSignature = rootSignatureManager->generateRootSignature("RasterRootSignature", rendererResources->pDevice, rootSignatureFlags);

	auto descHeapManager = DescriptorHeap(rootSignatureManager, "TexturesDescTable", "Textures1", rendererResources->pDevice);
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	for (const auto& texture : rendererResources->textures)
		descHeapManager.setSRV(0, srvDesc, rendererResources->pDevice, texture);
	rootDescriptorHeap = descHeapManager.getDescriptorHeap();


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

	D3D12_RT_FORMAT_ARRAY rtvFormats = {};
	rtvFormats.NumRenderTargets = 1;
	rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

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

	auto commandList = rRes->commandQueue->getCommandList();

	// Init const buffer
	constBuffer.numLights = static_cast<uint32_t>(rendererResources->scene->getLights().size());
	wrl::ComPtr<ID3D12Resource> cBuffIntBuffer;
	constantBuffer = DXUtil::uploadDataToDefaultHeap(
		rRes->pDevice,
		commandList,
		cBuffIntBuffer,
		&constBuffer,
		sizeof(constBuffer),
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	constantBuffer->SetName(L"Constant Buffer");

	auto fV = rRes->commandQueue->executeCommandList(commandList);
	rRes->commandQueue->waitForFenceValue(fV);
}

void RasterShading::draw(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, uint32_t currentBackBufferIndex)
{
	// Clear Depth
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvDescriptorHandle(pDepthDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), currentBackBufferIndex, dsvDescriptorSize);
	pCurrentCommandList->ClearDepthStencilView(dsvDescriptorHandle, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

	pCurrentCommandList->SetPipelineState(pipelineState.Get());
	pCurrentCommandList->SetGraphicsRootSignature(rootSignature.Get());

	pCurrentCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pCurrentCommandList->IASetVertexBuffers(0u, 3u, &bufferViews[0]);
	pCurrentCommandList->IASetIndexBuffer(&indexView);

	pCurrentCommandList->RSSetScissorRects(1u, &scissorRect);
	pCurrentCommandList->RSSetViewports(1u, &viewport);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle(rendererResources->pRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), currentBackBufferIndex, rendererResources->pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
	pCurrentCommandList->OMSetRenderTargets(1u, &rtvDescriptorHandle, FALSE, &dsvDescriptorHandle);

	// Update the MVP matrix
	constBuffer.ViewPerspective = rendererResources->camera->getViewPerspectiveMatrixColMajor();
	constBuffer.CameraPosition = rendererResources->camera->getPosition();
	DXUtil::updateDataInDefaultHeap(
		rendererResources->pDevice,
		pCurrentCommandList,
		constantBuffer,
		constantTempBuffer[currentBackBufferIndex],
		&constBuffer,
		sizeof(constBuffer),
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	pCurrentCommandList->SetGraphicsRootConstantBufferView(0u, constantBuffer->GetGPUVirtualAddress()); // Const buff (includes Cam)
	pCurrentCommandList->SetGraphicsRootShaderResourceView(2u, rendererResources->materialBuffer->GetGPUVirtualAddress());  // Material
	pCurrentCommandList->SetGraphicsRootShaderResourceView(3u, rendererResources->faceAttributeBuffer->GetGPUVirtualAddress());  // Face
	pCurrentCommandList->SetGraphicsRootShaderResourceView(4u, rendererResources->lightBuffer->GetGPUVirtualAddress());  // Light
	pCurrentCommandList->SetGraphicsRootShaderResourceView(5u, bufferViews[0].BufferLocation);  // Vertices
	pCurrentCommandList->SetGraphicsRootShaderResourceView(6u, bufferViews[1].BufferLocation);  // Tex
	pCurrentCommandList->SetGraphicsRootShaderResourceView(7u, bufferViews[2].BufferLocation);  // Normals
	pCurrentCommandList->SetGraphicsRootShaderResourceView(8u, indexView.BufferLocation);  // Indices
	pCurrentCommandList->SetGraphicsRootShaderResourceView(9u, rendererResources->matrices->GetGPUVirtualAddress());  // Matrices
	pCurrentCommandList->SetDescriptorHeaps(1u, rootDescriptorHeap.GetAddressOf());
	pCurrentCommandList->SetGraphicsRootDescriptorTable(10u, rootDescriptorHeap->GetGPUDescriptorHandleForHeapStart()); // Textures

	std::uint32_t i = 0;
	std::array<std::uint32_t, 2> constants;
	for (const auto &child : rendererResources->scene->getSceneGraph().Children)
	{
		auto& indexedSpan = rendererResources->scene->getMeshIndexedSpan(child.GroupName);
		constants = { i++, static_cast<UINT>(indexedSpan.Start) };
		pCurrentCommandList->SetGraphicsRoot32BitConstants(1u, 2u, constants.data(), 0u);
		pCurrentCommandList->DrawIndexedInstanced(static_cast<UINT>(indexedSpan.Size), 1u, static_cast<UINT>(indexedSpan.Start), 0u, 0u);
	}
}

void RasterShading::onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, uint32_t currentBackBufferIndex, ChangeEvent_t changeEvent)
{
}

void RasterShading::onResize()
{
	viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(rendererResources->winDimensions.x), static_cast<float>(rendererResources->winDimensions.y));
	pDepthBuffers = DXUtil::createDepthStencilView(rendererResources->pDevice, pDepthDescriptorHeap, rendererResources->winDimensions.x, rendererResources->winDimensions.y, rendererResources->numBackBuffers);
}
