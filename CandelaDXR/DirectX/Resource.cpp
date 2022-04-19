#include "Resource.h"

#include "d3dx12.h"
#include "DxUtil.h"

using candela::directx::Resource;
using candela::directx::DXResource;
using candela::directx::DXUtil;

Resource::Resource(DXResource resource, D3D12_RESOURCE_STATES state)
	: resource(resource), state(state)
{
	auto desc = resource->GetDesc();
	desc = desc;
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

Resource::operator DXResource()
{
	return resource;
}

Resource::operator ID3D12Resource* ()
{
	return resource.Get();
}

void Resource::setName(const std::wstring& name)
{
	resource->SetName(name.c_str());
}

std::wstring Resource::getName() const
{
	wchar_t name[128];
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

