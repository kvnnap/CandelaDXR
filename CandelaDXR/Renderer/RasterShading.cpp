#include <d3dcompiler.h>

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
using candela::directx::DXUtil;
using DirectX::XMMATRIX;
using std::uint32_t;
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
	dsvDescriptorSize = rRes->pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	pDepthBuffers = DXUtil::createDepthStencilView(rRes->pDevice, pDepthDescriptorHeap, rRes->winDimensions.x, rRes->winDimensions.y, rRes->numBackBuffers);

	//D3D12_INPUT_ELEMENT_DESC ied[] = {};

	// Root signature - https://docs.microsoft.com/en-us/windows/desktop/direct3d12/root-signatures-overview
	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(rRes->pDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	// Allow input layout and deny unnecessary access to certain pipeline stages.
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		| D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
		| D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
		| D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
		//| D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS
		;

	// A single 32-bit constant root parameter that is used by the vertex shader. (CAMERA)
	CD3DX12_ROOT_PARAMETER1 rootParameters[10] = {};
	//rootParameters[0].InitAsConstants(sizeof(XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
	rootParameters[0].InitAsConstantBufferView(0);
	rootParameters[1].InitAsShaderResourceView(0);
	rootParameters[2].InitAsShaderResourceView(1);
	rootParameters[3].InitAsShaderResourceView(2);
	rootParameters[4].InitAsShaderResourceView(3);
	rootParameters[5].InitAsShaderResourceView(4);
	rootParameters[6].InitAsShaderResourceView(5);
	rootParameters[7].InitAsShaderResourceView(6);
	rootParameters[8].InitAsShaderResourceView(7);
	rootParameters[9].InitAsConstants(2, 1);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
	rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

	// Serialize the root signature.
	wrl::ComPtr<ID3DBlob> rootSignatureBlob;
	wrl::ComPtr<ID3DBlob> errorBlob;
	GFXTHROWIFFAILED(D3DX12SerializeVersionedRootSignature(&rootSignatureDescription,
		featureData.HighestVersion, &rootSignatureBlob, &errorBlob));
	// Create the root signature.
	GFXTHROWIFFAILED(rRes->pDevice->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
		rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));

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
	wrl::ComPtr<ID3D12Resource> cBuffIntBuffer;
	constantBuffer = DXUtil::uploadDataToDefaultHeap(
		rRes->pDevice,
		commandList,
		cBuffIntBuffer,
		&constBuffer,
		sizeof(constBuffer),
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

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
	constBuffer.MVP = rendererResources->camera->getViewPerspectiveMatrix();
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
	pCurrentCommandList->SetGraphicsRootShaderResourceView(1u, rendererResources->materialBuffer->GetGPUVirtualAddress());  // Material
	pCurrentCommandList->SetGraphicsRootShaderResourceView(2u, rendererResources->faceAttributeBuffer->GetGPUVirtualAddress());  // Face
	pCurrentCommandList->SetGraphicsRootShaderResourceView(3u, rendererResources->lightBuffer->GetGPUVirtualAddress());  // Light
	pCurrentCommandList->SetGraphicsRootShaderResourceView(4u, bufferViews[0].BufferLocation);  // Vertices
	pCurrentCommandList->SetGraphicsRootShaderResourceView(5u, bufferViews[1].BufferLocation);  // Tex
	pCurrentCommandList->SetGraphicsRootShaderResourceView(6u, bufferViews[2].BufferLocation);  // Normals
	pCurrentCommandList->SetGraphicsRootShaderResourceView(7u, indexView.BufferLocation);  // Indices
	pCurrentCommandList->SetGraphicsRootShaderResourceView(8u, rendererResources->matrices->GetGPUVirtualAddress());  // Matrices

	std::uint32_t i = 0;
	std::array<std::uint32_t, 2> constants;
	for (const auto &child : rendererResources->scene->getSceneGraph().Children)
	{
		auto& indexedSpan = rendererResources->scene->getMeshIndexedSpan(child.GroupName);
		constants = { i++, static_cast<UINT>(indexedSpan.Start) };
		pCurrentCommandList->SetGraphicsRoot32BitConstants(9u, 2u, constants.data(), 0u);
		pCurrentCommandList->DrawIndexedInstanced(static_cast<UINT>(indexedSpan.Size), 1u, static_cast<UINT>(indexedSpan.Start), 0u, 0u);
	}
}

void RasterShading::onChange(wrl::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList, uint32_t currentBackBufferIndex)
{
}


