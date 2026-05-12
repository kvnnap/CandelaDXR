#include "Resource.h"

#include "d3dx12.h"
#include "DxUtil.h"

#include "CommandQueue.h"
#include "Exception/Exception.h"
#include "Util/StringUtil.h"

using std::vector;
using std::uint32_t;
using DirectX::XMFLOAT4;

using candela::mathematics::UVector2;

using candela::directx::Resource;
using candela::directx::ResourceData;
using candela::directx::DXResource;
using candela::directx::DXUtil;

Resource::Resource(DXResource resource, D3D12_RESOURCE_STATES state)
	: resource(resource), state(state), prevState(state)
{
}

void Resource::rewriteState(D3D12_RESOURCE_STATES currentState)
{
	prevState = state;
	state = currentState;
}

void Resource::transitionBarrier(DXCommandList& commandList, D3D12_RESOURCE_STATES nextState)
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), state, nextState);
	commandList->ResourceBarrier(1u, &barrier);
	prevState = state;
	state = nextState;
}

void Resource::transitionToPrevBarrier(DXCommandList& commandList)
{
	transitionBarrier(commandList, prevState);
}

void Resource::uavBarrier(DXCommandList& commandList)
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(resource.Get());
	commandList->ResourceBarrier(1u, &barrier);
}

D3D12_RESOURCE_STATES Resource::getState() const
{
	return state;
}

Resource::operator DXResource&()
{
	return resource;
}

Resource::operator const DXResource& () const
{
	return resource;
}

Resource::operator ID3D12Resource* ()
{
	return resource.Get();
}

ResourceData Resource::read(DXCommandQueue& commandQueue)
{
	// Get device
	DXDevice device;
	resource->GetDevice(IID_PPV_ARGS(&device));

	// Get resource info
	auto desc = resource->GetDesc();
	if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
		ThrowException("Only Texture2D currently supported");

	UINT numRows;
	UINT64 rowSizeInBytes, totalSize;
	device->GetCopyableFootprints(&desc, 0u, 1u, 0u, nullptr, &numRows, &rowSizeInBytes, &totalSize);
	UINT64 rowPitchSizeInBytes = (rowSizeInBytes + (D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u)) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u);
	
	// Create readback buffer
	auto commandList = commandQueue->getCommandList();
	auto res = createCommittedResource(device, totalSize, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_READBACK);
	const auto prevState = state;
	transitionBarrier(commandList, D3D12_RESOURCE_STATE_COPY_SOURCE);

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT bufferFootprint = {};
	bufferFootprint.Footprint.Width = static_cast<UINT>(desc.Width);
	bufferFootprint.Footprint.Height = desc.Height;
	bufferFootprint.Footprint.Depth = 1u;
	bufferFootprint.Footprint.RowPitch = static_cast<UINT>(rowPitchSizeInBytes);
	bufferFootprint.Footprint.Format = desc.Format;

	CD3DX12_TEXTURE_COPY_LOCATION Dst(res, bufferFootprint);
	CD3DX12_TEXTURE_COPY_LOCATION Src(resource.Get(), 0u);
	commandList->CopyTextureRegion(&Dst, 0u, 0u, 0u, &Src, nullptr);
	transitionBarrier(commandList, prevState);

	auto fenceValue = commandQueue->executeCommandList(commandList);
	commandQueue->waitForFenceValue(fenceValue);

	// Can now read data
	D3D12_RANGE destRange{ 0u, totalSize };
	vector<XMFLOAT4> data;
	auto floatsPerRow = rowSizeInBytes / sizeof(float); // decltype(data)::value_type
	data.reserve(rowSizeInBytes / sizeof(decltype(data)::value_type) * numRows);
	float* values{};
	res.resource->Map(0u, &destRange, reinterpret_cast<void**>(&values));

	auto skipAmount = rowPitchSizeInBytes / sizeof(float);
	
	// Select the pixel component mapping
	auto floatsPerPixel = floatsPerRow / desc.Width;
	UINT compSel[4] = { 0, 1, 2, 3 };
	if (floatsPerPixel == 1)
		compSel[1] = compSel[2] = compSel[3] = 0;
	else if (floatsPerPixel == 2)
	{
		compSel[2] = 0;
		compSel[3] = 1;
	}
	else if (floatsPerPixel == 3)
		compSel[3] = 0;

	// Copy it to our own buffer
	for (UINT r = 0; r < numRows; ++r)
	{
		for (UINT64 c = 0; c < floatsPerRow; c += floatsPerPixel)
			data.emplace_back(values[c + compSel[0]], values[c + compSel[1]], values[c + compSel[2]], values[c + compSel[3]]);
		values += skipAmount;
	}

	// Unmap
	destRange.End = 0u;
	res.resource->Unmap(0u, &destRange);

	return ResourceData{getName(), desc.Width, desc.Height, std::move(data)};
}

void Resource::write(DXCommandList& commandList, DXResource& tempResource, const void* ptData)
{
	// Get device
	DXDevice device;
	resource->GetDevice(IID_PPV_ARGS(&device));

	// Get resource info
	auto desc = resource->GetDesc();

	UINT64 rowSizeInBytes, totalSize;
	device->GetCopyableFootprints(&desc, 0u, 1u, 0u, nullptr, nullptr, &rowSizeInBytes, &totalSize);

	tempResource = DXUtil::createCommittedResource(device, D3D12_HEAP_TYPE_UPLOAD, totalSize, D3D12_RESOURCE_STATE_GENERIC_READ);
	D3D12_SUBRESOURCE_DATA subresourceData = {};
	subresourceData.pData = ptData;
	subresourceData.RowPitch = rowSizeInBytes;
	subresourceData.SlicePitch = subresourceData.RowPitch * desc.Height;

	auto prevState = getState();
	transitionBarrier(commandList, D3D12_RESOURCE_STATE_COPY_DEST);
	UpdateSubresources(commandList.Get(), *this, tempResource.Get(), 0, 0, 1, &subresourceData);
	transitionBarrier(commandList, prevState);
}

void Resource::resize(uint32_t width, uint32_t height)
{
	// Get resource info
	DXResource& dxRes = resource;
	auto desc = dxRes->GetDesc();
	if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
		ThrowException("resize error: Resource is not Texture2D");

	// Get device
	DXDevice device;
	dxRes->GetDevice(IID_PPV_ARGS(&device));

	// Resize the resource
	auto name = getName();
	resource = Resource::createTextureCommittedResource(
		device, width, height, state,
		desc.Format, desc.Flags).resource;
	setName(name);
}

void Resource::createShaderResourceView(D3D12_CPU_DESCRIPTOR_HANDLE heapSrvCpuDesc)
{
	// Get resource info
	DXResource& dxRes = resource;
	auto desc = dxRes->GetDesc();
	if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
		ThrowException("createShaderResourceView error: Resource is not Texture2D");

	// Get device
	DXDevice device;
	dxRes->GetDevice(IID_PPV_ARGS(&device));

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	if (desc.Format == DXGI_FORMAT_D32_FLOAT || desc.Format == DXGI_FORMAT_R32_FLOAT)
	{
		srvDesc.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(0, 0, 0, 5);
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	}
	else
	{
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = desc.Format;
	}
	
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = desc.MipLevels;

	device->CreateShaderResourceView(*this, &srvDesc, heapSrvCpuDesc);
}

float Resource::getAspectRatio()
{
	DXResource& dxRes = resource;
	auto desc = dxRes->GetDesc();
	if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
		ThrowException("createShaderResourceView error: Resource is not Texture2D");
	return static_cast<float>(desc.Width) / static_cast<float>(desc.Height);
}

UVector2 Resource::getDimensions()
{
	DXResource& dxRes = resource;
	auto desc = dxRes->GetDesc();
	if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
		ThrowException("createShaderResourceView error: Resource is not Texture2D");
	return mathematics::UVector2(static_cast<UINT>(desc.Width), desc.Height);
}

void Resource::setResource(DXResource p_resource, D3D12_RESOURCE_STATES p_state)
{
	resource = p_resource;
	state = p_state;
}

void Resource::setName(const std::wstring& name)
{
	resource->SetName(name.c_str());
}

void Resource::setName(const std::string& name)
{
	setName(util::StringToWString(name));
}

std::wstring Resource::getName() const
{
	wchar_t name[128];
	name[0] = 0;
	UINT nameSize = sizeof(name);
	resource->GetPrivateData(WKPDID_D3DDebugObjectNameW, &nameSize, name);
	return name;
}

// Factories
Resource Resource::createTextureCommittedResource(DXDevice& device, UINT64 width, UINT height, D3D12_RESOURCE_STATES resourceState, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS resourceFlags, D3D12_HEAP_TYPE heapType, D3D12_CLEAR_VALUE* clearValue)
{
	return Resource(DXUtil::createTextureCommittedResource(device, heapType, width, height, resourceState, resourceFlags, format, clearValue), resourceState);
}

Resource Resource::createCommittedResource(DXDevice& device, UINT64 size, D3D12_RESOURCE_STATES resourceState, D3D12_RESOURCE_FLAGS resourceFlags, D3D12_HEAP_TYPE heapType, D3D12_CLEAR_VALUE* clearValue)
{
	return Resource(DXUtil::createCommittedResource(device, heapType, size, resourceState, resourceFlags, clearValue), resourceState);
}

