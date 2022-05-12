#include "Resource.h"

#include "d3dx12.h"
#include "DxUtil.h"

#include "CommandQueue.h"
#include "Exception/Exception.h"

using std::vector;
using std::uint32_t;
using DirectX::XMFLOAT4;

using candela::directx::Resource;
using candela::directx::ResourceData;
using candela::directx::DXResource;
using candela::directx::DXUtil;

Resource::Resource(DXResource resource, D3D12_RESOURCE_STATES state)
	: resource(resource), state(state)
{
}

void Resource::transistionBarrier(DXCommandList& commandList, D3D12_RESOURCE_STATES nextState)
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), state, nextState);
	commandList->ResourceBarrier(1u, &barrier);
	state = nextState;
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
	transistionBarrier(commandList, D3D12_RESOURCE_STATE_COPY_SOURCE);

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT bufferFootprint = {};
	bufferFootprint.Footprint.Width = static_cast<UINT>(desc.Width);
	bufferFootprint.Footprint.Height = desc.Height;
	bufferFootprint.Footprint.Depth = 1u;
	bufferFootprint.Footprint.RowPitch = static_cast<UINT>(rowPitchSizeInBytes);
	bufferFootprint.Footprint.Format = desc.Format;

	CD3DX12_TEXTURE_COPY_LOCATION Dst(res, bufferFootprint);
	CD3DX12_TEXTURE_COPY_LOCATION Src(resource.Get(), 0u);
	commandList->CopyTextureRegion(&Dst, 0u, 0u, 0u, &Src, nullptr);
	transistionBarrier(commandList, prevState);

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

	// Copy it to our own buffer
	for (UINT r = 0; r < numRows; ++r)
	{
		for (UINT64 c = 0; c < floatsPerRow; c += 4)
			data.emplace_back(values[c], values[c + 1], values[c + 2], values[c + 3]);
		values += skipAmount;
	}

	// Unmap
	destRange.End = 0u;
	res.resource->Unmap(0u, &destRange);

	return ResourceData{desc.Width, desc.Height, std::move(data)};
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
	resource = Resource::createTextureCommittedResource(
		device, width, height, state,
		desc.Format, desc.Flags).resource;
}

void Resource::setName(const std::wstring& name)
{
	resource->SetName(name.c_str());
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
Resource Resource::createTextureCommittedResource(DXDevice& device, UINT64 width, UINT height, D3D12_RESOURCE_STATES resourceState, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS resourceFlags, D3D12_HEAP_TYPE heapType)
{
	return Resource(DXUtil::createTextureCommittedResource(device, heapType, width, height, resourceState, resourceFlags, format), resourceState);
}

Resource Resource::createCommittedResource(DXDevice& device, UINT64 size, D3D12_RESOURCE_STATES resourceState, D3D12_RESOURCE_FLAGS resourceFlags, D3D12_HEAP_TYPE heapType)
{
	return Resource(DXUtil::createCommittedResource(device, heapType, size, resourceState, resourceFlags), resourceState);
}

