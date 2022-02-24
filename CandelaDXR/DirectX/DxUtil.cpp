#include "DXUtil.h"

#include <array>
#include <iostream>
#include <DirectXMath.h>

#include "d3dx12.h"

#include "Exception/WindowException.h"
#include "Exception/DxgiInfoException.h"

using std::array;
using std::vector;
using std::string;
using std::cout;
using std::endl;
using std::uint32_t;

using Microsoft::WRL::ComPtr;

using candela::directx::DXUtil;

void DXUtil::enableDebugLayer()
{
#ifdef _DEBUG
	HRESULT hr;
	ComPtr<ID3D12Debug> debugInterface;
	GFXTHROWIFFAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
	debugInterface->EnableDebugLayer();
#endif
}

void DXUtil::setupDebugLayer(ComPtr<ID3D12Device> pDevice)
{
#ifdef _DEBUG
	HRESULT hr;
	ComPtr<ID3D12InfoQueue> infoQueue;
	GFXTHROWIFFAILED(pDevice.As(&infoQueue));
	// Uncomment these to break at the exact location (requires attache debugger)
	// With no debugger, the app exits immediately on the spot.
	//infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
	//infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
	//infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

	D3D12_INFO_QUEUE_FILTER queueFilter = {};
	D3D12_MESSAGE_SEVERITY severities[] = {
		D3D12_MESSAGE_SEVERITY_INFO
	};
	D3D12_MESSAGE_ID denyIds[] = {
		D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE
	};

	// TODO comment the below to see if we get more messages
	queueFilter.DenyList.pSeverityList = severities;
	queueFilter.DenyList.NumSeverities = static_cast<UINT>(std::size(severities));
	queueFilter.DenyList.pIDList = denyIds;
	queueFilter.DenyList.NumIDs = static_cast<UINT>(std::size(denyIds));

	GFXTHROWIFFAILED(infoQueue->PushStorageFilter(&queueFilter));

#endif
}

bool DXUtil::checkTearingSupport()
{
	BOOL allowTearing = false;
	UINT createFactoryFlags = 0;

	auto dxgiFactory = createDXGIFactory();

	HRESULT hr;

	ComPtr<IDXGIFactory5> dxgiFactory5;
	GFXTHROWIFFAILED(dxgiFactory.As(&dxgiFactory5));
	GFXTHROWIFFAILED(dxgiFactory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing)));

	return allowTearing == TRUE;
}

ComPtr<IDXGIAdapter> DXUtil::getAdapterLatestFeatureLevel(D3D_FEATURE_LEVEL* fl, bool useWarp, std::uint32_t adapterIndex)
{
	// Get DX12 compatible hardware device - Adapter contains info about the actual device
	struct FeatureLevelItem
	{
		D3D_FEATURE_LEVEL FeatureLevel;
		string Name;
	};

	array<FeatureLevelItem, 5> featureLevels = {
		FeatureLevelItem { D3D_FEATURE_LEVEL_12_2, "D3D_FEATURE_LEVEL_12_2"},
		FeatureLevelItem { D3D_FEATURE_LEVEL_12_1, "D3D_FEATURE_LEVEL_12_1"},
		FeatureLevelItem { D3D_FEATURE_LEVEL_12_0, "D3D_FEATURE_LEVEL_12_0"},
		FeatureLevelItem { D3D_FEATURE_LEVEL_11_1, "D3D_FEATURE_LEVEL_11_1"},
		FeatureLevelItem { D3D_FEATURE_LEVEL_11_0, "D3D_FEATURE_LEVEL_11_0"}
	};

	vector<ComPtr<IDXGIAdapter>> adapters;
	for (auto featureLevel : featureLevels)
	{
		cout << "Trying " << featureLevel.Name << " ... ";
		adapters = getAdapters(featureLevel.FeatureLevel);
		if (adapters.empty()) 
		{
			cout << "FAILED" << endl;
		}
		else {
			*fl = featureLevel.FeatureLevel;
			cout << "OK" << endl;
			break;
		}
	}
	
	if (adapters.empty())
		ThrowException("Cannot find a compatible DX12 hardware device");

	cout << "Compatible adapters: " << endl;

	std::uint32_t i{};
	for (auto adapter : adapters)
	{
		DXGI_ADAPTER_DESC desc;
		adapter->GetDesc(&desc);
		wprintf(L"[%d] - %s\n", i++, desc.Description);
		//wprintf(L"[%d] - %s - RT [%c]\n", i++, desc.Description, checkDeviceRTSupport(adapter, *fl) ? 'Y' : 'N');
	}

	cout << "Using adapter: " << adapterIndex << endl;

	if (adapterIndex >= adapters.size())
		ThrowException("Adapter index out of range");

	return adapters.at(adapterIndex);
}

vector<ComPtr<IDXGIAdapter>> DXUtil::getAdapters(D3D_FEATURE_LEVEL featureLevel, bool useWarp)
{
	auto dxgiFactory = createDXGIFactory();

	vector<ComPtr<IDXGIAdapter>> adapters;
	ComPtr<IDXGIAdapter1> dxgiAdapter1;
	ComPtr<IDXGIAdapter4> dxgiAdapter4;
	HRESULT hr;
	if (useWarp)
	{
		ComPtr<IDXGIFactory4> dxgiFactory4;
		GFXTHROWIFFAILED(dxgiFactory.As(&dxgiFactory4));
		GFXTHROWIFFAILED(dxgiFactory4->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter4)));
		adapters.push_back(dxgiAdapter4);
	}
	else {
		ComPtr<IDXGIFactory1> dxgiFactory1;
		GFXTHROWIFFAILED(dxgiFactory.As(&dxgiFactory1));

		for (UINT adapterIndex = 0; dxgiFactory1->EnumAdapters1(adapterIndex, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++adapterIndex)
		{
			DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
			dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);
			const bool isHardware = (dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0;
			const bool d3d12DeviceCreationSuccess = SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(), featureLevel, __uuidof(ID3D12Device), nullptr));
			if (isHardware && d3d12DeviceCreationSuccess)
			{
				GFXTHROWIFFAILED(dxgiAdapter1.As(&dxgiAdapter4));
				adapters.push_back(dxgiAdapter4);
			}
		}
	}

	return adapters;
}

bool DXUtil::checkDeviceRTSupport(ComPtr<ID3D12Device> device)
{
	HRESULT hr;
	D3D12_FEATURE_DATA_D3D12_OPTIONS5 features5 = {};
	GFXTHROWIFFAILED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features5, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5)));
	return features5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
}

bool DXUtil::checkDeviceRTSupport(ComPtr<IDXGIAdapter> adapter, D3D_FEATURE_LEVEL featureLevel)
{
	HRESULT hr;
	D3D12_FEATURE_DATA_D3D12_OPTIONS5 features5 = {};
	GFXTHROWIFFAILED(createDeviceFromAdapter(adapter, featureLevel)->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features5, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5)));
	return features5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
}

ComPtr<ID3D12Device> DXUtil::createDeviceFromAdapter(ComPtr<IDXGIAdapter> adapter, D3D_FEATURE_LEVEL featureLevel)
{
	HRESULT hr;
	ComPtr<ID3D12Device> pDevice;
	GFXTHROWIFFAILED(D3D12CreateDevice(adapter.Get(), featureLevel, IID_PPV_ARGS(&pDevice)));
	return pDevice;
}

ComPtr<IDXGIFactory> DXUtil::createDXGIFactory()
{
	HRESULT hr;
	ComPtr<IDXGIFactory> dxgiFactory;
	//dxgiFactory->
	UINT createFactoryFlags = 0;
#ifdef _DEBUG
	createFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
	GFXTHROWIFFAILED(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));
	return dxgiFactory;
}

ComPtr<ID3D12DescriptorHeap> DXUtil::createDescriptorHeap(ComPtr<ID3D12Device> device, UINT count, D3D12_DESCRIPTOR_HEAP_TYPE type, bool shaderVisible)
{
	HRESULT hr;
	ComPtr<ID3D12DescriptorHeap> descriptorHeap;

	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = count;
	desc.Type = type;
	desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	GFXTHROWIFFAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)));

	return descriptorHeap;
}

ComPtr<IDXGISwapChain> DXUtil::createSwapChain(ComPtr<ID3D12CommandQueue> commandQueue, HWND hWnd, UINT numBuffers)
{
	DXGI_SWAP_CHAIN_DESC1 sd = {};
	// Use the window (hWnd) dimensions
	sd.Width = 0;
	sd.Height = 0;

	// Pixel format (TODO: Try srgb too)
	sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.Stereo = FALSE;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = numBuffers;
	sd.Scaling = DXGI_SCALING_NONE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	sd.Flags = checkTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	ComPtr<IDXGISwapChain4> swapChain;
	ComPtr<IDXGISwapChain1> swapChain1;

	HRESULT hr;
	ComPtr<IDXGIFactory2> dxgiFactory2;
	GFXTHROWIFFAILED(createDXGIFactory().As(&dxgiFactory2));
	GFXTHROWIFFAILED(dxgiFactory2->CreateSwapChainForHwnd(commandQueue.Get(), hWnd, &sd, nullptr, nullptr, &swapChain1));
	GFXTHROWIFFAILED(swapChain1.As(&swapChain));
	// TODO: pCurrentBackBufferIndex = swapChain->GetCurrentBackBufferIndex();

	return swapChain;
}

std::vector<ComPtr<ID3D12Resource>> DXUtil::createRenderTargetViews(
	ComPtr<ID3D12Device> device,
	ComPtr<ID3D12DescriptorHeap> descriptorHeap,
	ComPtr<IDXGISwapChain> swapChain,
	UINT numRTV)
{
	std::vector<ComPtr<ID3D12Resource>> backBuffers(numRTV);

	auto pRTVDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(descriptorHeap->GetCPUDescriptorHandleForHeapStart());

	HRESULT hr;
	for (UINT i = 0; i < numRTV; ++i) {
		GFXTHROWIFFAILED(swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffers[i])));
		// MAY FAIL -------
		/*ThrowDxgiInfoExceptionIfFailed*/(device->CreateRenderTargetView(backBuffers[i].Get(), nullptr, rtvHandle));
		// MAY FAIL END ---
		rtvHandle.Offset(pRTVDescriptorSize);
	}

	return backBuffers;
}

std::vector<ComPtr<ID3D12Resource>> DXUtil::createDepthStencilView(
	ComPtr<ID3D12Device> device,
	ComPtr<ID3D12DescriptorHeap> depthDescriptorHeap,
	UINT winWidth, UINT winHeight,
	UINT numDSV)
{
	std::vector<ComPtr<ID3D12Resource>> depthBuffers(numDSV);

	auto pDSVDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(depthDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = DXGI_FORMAT_D32_FLOAT;
	clearValue.DepthStencil.Depth = 1.f;
	clearValue.DepthStencil.Stencil = 0;

	D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
	dsv.Format = DXGI_FORMAT_D32_FLOAT;
	dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsv.Texture2D.MipSlice = 0;
	dsv.Flags = D3D12_DSV_FLAG_NONE;

	HRESULT hr;
	for (UINT i = 0; i < numDSV; ++i) {
		// Create depth buffer
		auto heapPropDesc = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		auto tex2DDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, winWidth, winHeight, 1u, 0u, 1u, 0u, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
		GFXTHROWIFFAILED(device->CreateCommittedResource(
			&heapPropDesc,
			D3D12_HEAP_FLAG_NONE,
			&tex2DDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&clearValue,
			IID_PPV_ARGS(&depthBuffers[i])
		));

		// Create view for resource
		/*ThrowDxgiInfoExceptionIfFailed*/(device->CreateDepthStencilView(depthBuffers[i].Get(), &dsv, dsvHandle));
		dsvHandle.Offset(pDSVDescriptorSize);
	}

	return depthBuffers;
}

ComPtr<ID3D12Resource> DXUtil::createCommittedResource(ComPtr<ID3D12Device> device, D3D12_HEAP_TYPE heapType, UINT64 size, D3D12_RESOURCE_STATES resourceState, D3D12_RESOURCE_FLAGS resourceFlags)
{
	ComPtr<ID3D12Resource> buffer;

	HRESULT hr;
	auto heapPropDesc = CD3DX12_HEAP_PROPERTIES(heapType);
	auto resBuffDesc = CD3DX12_RESOURCE_DESC::Buffer(size, resourceFlags);
	GFXTHROWIFFAILED(device->CreateCommittedResource(
		&heapPropDesc,
		D3D12_HEAP_FLAG_NONE,
		&resBuffDesc,
		resourceState,
		nullptr,
		IID_PPV_ARGS(&buffer)
	));

	return buffer;
}

ComPtr<ID3D12Resource> DXUtil::createTextureCommittedResource(ComPtr<ID3D12Device> device, D3D12_HEAP_TYPE heapType, UINT64 width, UINT height, D3D12_RESOURCE_STATES resourceState, D3D12_RESOURCE_FLAGS resourceFlags, DXGI_FORMAT format)
{
	ComPtr<ID3D12Resource> buffer;

	// TODO: Check - Do we need mip level 1 for Ray Tracing?
	HRESULT hr;
	auto heapPropDesc = CD3DX12_HEAP_PROPERTIES(heapType);
	auto tex2DDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1u, 1u, 1u, 0u, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	GFXTHROWIFFAILED(device->CreateCommittedResource(
		&heapPropDesc,
		D3D12_HEAP_FLAG_NONE,
		&tex2DDesc,
		resourceState,
		nullptr,
		IID_PPV_ARGS(&buffer)
	));

	return buffer;
}

ComPtr<ID3D12Resource> DXUtil::uploadDataToDefaultHeap(ComPtr<ID3D12Device> pDevice, ComPtr<ID3D12GraphicsCommandList> pCommandList, ComPtr<ID3D12Resource>& tempResource, const void* ptData, std::size_t dataSize, D3D12_RESOURCE_STATES finalState)
{
	// Upload buffer to gpu
	ComPtr<ID3D12Resource> defaultResource = DXUtil::createCommittedResource(pDevice, D3D12_HEAP_TYPE_DEFAULT, dataSize, D3D12_RESOURCE_STATE_COPY_DEST);

	updateDataInDefaultHeap(pDevice, pCommandList, defaultResource, tempResource, ptData, dataSize, D3D12_RESOURCE_STATE_COPY_DEST, finalState);

	return defaultResource;
}

ComPtr<ID3D12Resource> DXUtil::uploadTextureDataToDefaultHeap(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList> pCommandList, ComPtr<ID3D12Resource>& tempResource, const void* ptData, std::size_t width, std::size_t height, std::size_t sizePerPixel, DXGI_FORMAT format, D3D12_RESOURCE_STATES finalState)
{
	// create texture
	ComPtr<ID3D12Resource> texResource = createTextureCommittedResource(device, D3D12_HEAP_TYPE_DEFAULT, width, static_cast<UINT>(height), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_FLAG_NONE, format);

	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texResource.Get(), 0, 1);
	//const UINT64 uploadBufferSize = width * height * 4;

	// Upload buffer to gpu
	tempResource = DXUtil::createCommittedResource(device, D3D12_HEAP_TYPE_UPLOAD, uploadBufferSize, D3D12_RESOURCE_STATE_GENERIC_READ);
	D3D12_SUBRESOURCE_DATA subresourceData = {};
	subresourceData.pData = ptData;
	subresourceData.RowPitch = width * sizePerPixel;
	subresourceData.SlicePitch = subresourceData.RowPitch * height;
	UpdateSubresources(pCommandList.Get(), texResource.Get(), tempResource.Get(), 0, 0, 1, &subresourceData);

	// Change state so that it can be read
	auto resBarrierDesc = CD3DX12_RESOURCE_BARRIER::Transition(texResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, finalState);
	pCommandList->ResourceBarrier(1, &resBarrierDesc);

	return texResource;
}

void DXUtil::updateDataInDefaultHeap(ComPtr<ID3D12Device> pDevice, ComPtr<ID3D12GraphicsCommandList> pCommandList, ComPtr<ID3D12Resource>& resource, ComPtr<ID3D12Resource>& tempResource, const void* ptData, std::size_t dataSize, D3D12_RESOURCE_STATES previousState, D3D12_RESOURCE_STATES finalState)
{
	// Transition to correct state
	if (previousState != D3D12_RESOURCE_STATE_COPY_DEST) {
		auto resBarrierDesc = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), previousState, D3D12_RESOURCE_STATE_COPY_DEST);
		pCommandList->ResourceBarrier(1, &resBarrierDesc);
	}

	// Upload buffer to gpu
	tempResource = DXUtil::createCommittedResource(pDevice, D3D12_HEAP_TYPE_UPLOAD, dataSize, D3D12_RESOURCE_STATE_GENERIC_READ);
	D3D12_SUBRESOURCE_DATA subresourceData = {};
	subresourceData.pData = ptData;
	subresourceData.RowPitch = dataSize;
	subresourceData.SlicePitch = subresourceData.RowPitch;
	UpdateSubresources(pCommandList.Get(), resource.Get(), tempResource.Get(), 0, 0, 1, &subresourceData);

	// Change state so that it can be read
	auto resBarrierDesc = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, finalState);
	pCommandList->ResourceBarrier(1, &resBarrierDesc);
}


ComPtr<ID3D12RootSignature> DXUtil::createRootSignature(ComPtr<ID3D12Device> pDevice, const D3D12_VERSIONED_ROOT_SIGNATURE_DESC& rootSignatureDesc)
{
	// Check which root signature version we support - 1.1 is better than 1.0...
	// Root signature - https://docs.microsoft.com/en-us/windows/desktop/direct3d12/root-signatures-overview
	HRESULT hr;

	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(pDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	// Serialise the signature
	ComPtr<ID3DBlob> rootSignatureBlob;
	ComPtr<ID3DBlob> errorBlob;
	GFXTHROWIFFAILED(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &rootSignatureBlob, &errorBlob));

	// Create the root signature.
	ComPtr<ID3D12RootSignature> rootSignature;
	GFXTHROWIFFAILED(pDevice->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));

	return rootSignature;
}

ComPtr<ID3D12Device> DXUtil::createRTDeviceFromAdapter(ComPtr<IDXGIAdapter> adapter, D3D_FEATURE_LEVEL featureLevel)
{
	ComPtr<ID3D12Device> pDevice = createDeviceFromAdapter(adapter, featureLevel);
	if (!checkDeviceRTSupport(pDevice))
		ThrowException("Ray tracing is not supported on this device");
	return pDevice;
}

DXUtil::AccelerationStructureBuffers DXUtil::createBottomLevelAS(
	ComPtr<ID3D12Device> pDevice,
	ComPtr<ID3D12GraphicsCommandList> pCommandList,
	const std::vector<BottomLevelAccelerationData>& blasData,
	UINT vertexSize)
{
	AccelerationStructureBuffers blasBuffers;

	// Create geometry descriptors
	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> rtGeoDescriptors;
	rtGeoDescriptors.reserve(blasData.size());

	for (const auto& blasDatum : blasData) {
		// Acceleration Structure setup - describes our geometry (similar to ied)
		D3D12_RAYTRACING_GEOMETRY_DESC geometryDescriptor = {};
		geometryDescriptor.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		geometryDescriptor.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
		geometryDescriptor.Triangles.VertexBuffer.StartAddress = blasDatum.vertexBuffer;
		geometryDescriptor.Triangles.VertexBuffer.StrideInBytes = vertexSize;
		geometryDescriptor.Triangles.VertexCount = blasDatum.vertexCount;
		geometryDescriptor.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
		geometryDescriptor.Triangles.IndexBuffer = blasDatum.indexBuffer;
		geometryDescriptor.Triangles.IndexCount = blasDatum.indexCount;
		geometryDescriptor.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
		rtGeoDescriptors.push_back(geometryDescriptor);
	}

	// Bottom level?
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS rtStructureDescriptor = {};
	rtStructureDescriptor.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	rtStructureDescriptor.NumDescs = static_cast<UINT>(rtGeoDescriptors.size());
	rtStructureDescriptor.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	rtStructureDescriptor.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
	rtStructureDescriptor.pGeometryDescs = rtGeoDescriptors.data();

	// Query the buffer sizes that we need to allocate
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo = {};
	HRESULT hr;
	ComPtr<ID3D12Device5> pDevice5;
	GFXTHROWIFFAILED(pDevice.As(&pDevice5));

	pDevice5->GetRaytracingAccelerationStructurePrebuildInfo(&rtStructureDescriptor, &prebuildInfo);

	// Create the buffers..
	blasBuffers.pScratch = createCommittedResource(pDevice, D3D12_HEAP_TYPE_DEFAULT, prebuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	blasBuffers.pResult = createCommittedResource(pDevice, D3D12_HEAP_TYPE_DEFAULT, prebuildInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blasDesc = {};
	blasDesc.Inputs = rtStructureDescriptor;
	blasDesc.DestAccelerationStructureData = blasBuffers.pResult->GetGPUVirtualAddress();
	blasDesc.ScratchAccelerationStructureData = blasBuffers.pScratch->GetGPUVirtualAddress();
	ComPtr<ID3D12GraphicsCommandList4> pCommandList4;
	GFXTHROWIFFAILED(pCommandList.As(&pCommandList4));
	pCommandList4->BuildRaytracingAccelerationStructure(&blasDesc, 0, nullptr);

	return blasBuffers;
}

void DXUtil::buildTopLevelAS(
	ComPtr<ID3D12Device> pDevice,
	ComPtr<ID3D12GraphicsCommandList> pCommandList,
	const std::vector<TopLevelAccelerationData>& instanceData,
	ComPtr<ID3D12Resource>& tlasTempBuffer,
	bool update,
	DXUtil::AccelerationStructureBuffers& tlasBuffers)
{
	// Query the buffer sizes that we need to allocate
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS rtStructureDescriptor = {};
	rtStructureDescriptor.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	rtStructureDescriptor.NumDescs = static_cast<UINT>(instanceData.size());
	rtStructureDescriptor.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	rtStructureDescriptor.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;

	// Query the buffer sizes that we need to allocate
	HRESULT hr;
	ComPtr<ID3D12Device5> pDevice5;
	GFXTHROWIFFAILED(pDevice.As(&pDevice5));
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo = {};
	pDevice5->GetRaytracingAccelerationStructurePrebuildInfo(&rtStructureDescriptor, &prebuildInfo);

	if (update)
	{
		auto resBarrierDesc = CD3DX12_RESOURCE_BARRIER::UAV(tlasBuffers.pResult.Get());
		pCommandList->ResourceBarrier(1, &resBarrierDesc);
	}
	else {
		// Create the buffers..
		tlasBuffers.pScratch = createCommittedResource(pDevice, D3D12_HEAP_TYPE_DEFAULT, prebuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		tlasBuffers.pResult = createCommittedResource(pDevice, D3D12_HEAP_TYPE_DEFAULT, prebuildInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	}

	std::vector<D3D12_RAYTRACING_INSTANCE_DESC> rtInstanceDescs;
	rtInstanceDescs.resize(instanceData.size());

	for (size_t i = 0; i < instanceData.size(); ++i)
	{
		D3D12_RAYTRACING_INSTANCE_DESC& rtInstanceDesc = rtInstanceDescs[i];
		rtInstanceDesc.InstanceID = instanceData[i].instanceId;
		rtInstanceDesc.InstanceContributionToHitGroupIndex = 0;
		rtInstanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
		rtInstanceDesc.AccelerationStructure = instanceData[i].blasBuffer.pResult->GetGPUVirtualAddress();
		memcpy(rtInstanceDesc.Transform, &instanceData[i].transform, sizeof(rtInstanceDesc.Transform));
		rtInstanceDesc.InstanceMask = 0xFF;
	}

	// Upload ray tracing instance desc to GPU
	if (update)
		updateDataInDefaultHeap(pDevice, pCommandList, tlasBuffers.pInstanceDesc, tlasTempBuffer, rtInstanceDescs.data(), sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * rtInstanceDescs.size(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	else
		tlasBuffers.pInstanceDesc = uploadDataToDefaultHeap(pDevice, pCommandList, tlasTempBuffer, rtInstanceDescs.data(), sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * rtInstanceDescs.size(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	rtStructureDescriptor.InstanceDescs = tlasBuffers.pInstanceDesc->GetGPUVirtualAddress();

	// Descriptor for building TLAS
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlasDesc = {};
	tlasDesc.Inputs = rtStructureDescriptor;
	tlasDesc.DestAccelerationStructureData = tlasBuffers.pResult->GetGPUVirtualAddress();
	tlasDesc.ScratchAccelerationStructureData = tlasBuffers.pScratch->GetGPUVirtualAddress();

	if (update)
	{
		tlasDesc.Inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
		tlasDesc.SourceAccelerationStructureData = tlasBuffers.pResult->GetGPUVirtualAddress();
	}

	ComPtr<ID3D12GraphicsCommandList4> pCommandList4;
	GFXTHROWIFFAILED(pCommandList.As(&pCommandList4));
	pCommandList4->BuildRaytracingAccelerationStructure(&tlasDesc, 0, nullptr);

	// Insert barrier for uav access..
	auto resBarrierDesc = CD3DX12_RESOURCE_BARRIER::UAV(tlasBuffers.pResult.Get());
	pCommandList->ResourceBarrier(1, &resBarrierDesc);
}
