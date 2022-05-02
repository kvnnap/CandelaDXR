#include "ShadingTable.h"

#include "DXUtil.h"
#include <stdexcept>
#include <algorithm>
#include <locale>

#include "Util/StringUtil.h"

using std::string;
using std::to_string;
using std::wstring;
using std::runtime_error;
using std::numeric_limits;
using std::unique_ptr;
using std::shared_ptr;
using std::make_unique;
using std::make_shared;
using std::vector;
using std::max;
using std::unordered_map;

using candela::directx::ShadingTable;
using candela::directx::ShadingRecordType;
using candela::directx::DescriptorHeap;
using candela::directx::RootSignatureManager;

using candela::util::WStringToString;

ShadingTable::ShadingTable(std::shared_ptr<RootSignatureManager> rootSignatureManager)
	: rootSignatureManager(rootSignatureManager), tableLayout {}//, numHitGroupGeometries()
{}

void ShadingTable::addProgram(const wstring& programName, ShadingRecordType shadingRecordType, const string& rootSignatureName)
{
	shadingRecordsMap[programName] = shadingRecords.size();
	shadingRecords.push_back({ programName, rootSignatureName, shadingRecordType });
}

DescriptorHeap& ShadingTable::generateDescriptorHeap(const std::string& parameterName, const std::string& instanceName, Microsoft::WRL::ComPtr<ID3D12Device> pDevice)
{
	return *descriptorHeaps.emplace(instanceName, make_shared<DescriptorHeap>(rootSignatureManager, parameterName, instanceName, pDevice)).first->second;
}

void ShadingTable::addDescriptorHeap(std::shared_ptr<DescriptorHeap> descriptorHeap)
{
	if (rootSignatureManager != descriptorHeap->getRootSignatureManager())
		throw runtime_error("Descriptor Heap has a different root signature than this shading table");
	descriptorHeaps.emplace(descriptorHeap->getInstanceName(), descriptorHeap);
}

void ShadingTable::setInputForDescriptorTableParameter(const std::wstring& programName, const std::string& parameterName, const std::string& instanceName)
{
	// Get Program data
	auto& programStruct = shadingRecords[shadingRecordsMap.at(programName)];

	// Check if correct type
	const auto& parameter = rootSignatureManager->getParameterForRootSignature(programStruct.rootSignatureName, parameterName);
	if (parameter.ParameterType != D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
		throw runtime_error("Paramter '" + parameterName + "' not a descriptor table type");

	// Set
	programStruct.managedDescriptorHeapMap[parameterName] = descriptorHeaps.at(instanceName).get();
}

void ShadingTable::setInputForViewParameter(const wstring& programName, const std::string& parameterName, Microsoft::WRL::ComPtr<ID3D12Resource> resource, UINT64 offsetInBytes)
{
	// Get Program data
	auto& programStruct = shadingRecords[shadingRecordsMap.at(programName)];

	// Check if correct type
	const auto& parameter = rootSignatureManager->getParameterForRootSignature(programStruct.rootSignatureName, parameterName);
	if (parameter.ParameterType != D3D12_ROOT_PARAMETER_TYPE_CBV && parameter.ParameterType != D3D12_ROOT_PARAMETER_TYPE_SRV && parameter.ParameterType != D3D12_ROOT_PARAMETER_TYPE_UAV)
		throw runtime_error("Paramter '" + parameterName + "' not a CBV/SRV/UAV type");

	// Set
	programStruct.viewsMap[parameterName] = ViewStruct{ resource, offsetInBytes };
}

void ShadingTable::setInputForConstantParameter(const wstring& programName, const std::string& parameterName, UINT32 constant)
{
	// Get Program data
	auto& programStruct = shadingRecords[shadingRecordsMap.at(programName)];

	// Check if correct type
	const auto& parameter = rootSignatureManager->getParameterForRootSignature(programStruct.rootSignatureName, parameterName);
	if (parameter.ParameterType != D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
		throw runtime_error("Paramter '" + parameterName + "' not a 32bit constant type");

	// Set
	programStruct.constantsMap[parameterName] = constant;
}

Microsoft::WRL::ComPtr<ID3D12Resource> ShadingTable::generateShadingTable(
	Microsoft::WRL::ComPtr<ID3D12Device> pDevice,
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList,
	Microsoft::WRL::ComPtr<ID3D12StateObject> pStateObject,
	Microsoft::WRL::ComPtr<ID3D12Resource>& shadingTableTempResource)
{
	validateInputs();
	sortShadingRecords();

	// Calculate size per record type and total table size
	size_t totalTableSize = 0;
	for (size_t i = 0; i < std::size(tableLayout); ++i) {
		tableLayout[i].offsetInBytes = totalTableSize;

		// Assuming sorted shading records, find respective amounts
		size_t typeCount = 0;
		for (const auto& shadingRecord : shadingRecords) {
			if (shadingRecord.shadingRecordType == static_cast<ShadingRecordType>(i)) {
				++typeCount;
			}
			else if (typeCount > 0) {
				break;
			}
		}

		tableLayout[i].numOfRecords = typeCount;
		tableLayout[i].alignedRecordSize = getLargestRecordTypeSize(static_cast<ShadingRecordType>(i));

		// aligned record size
		auto t = numeric_limits<size_t>::max();
		std::align(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, tableLayout[i].alignedRecordSize, (void*&)tableLayout[i].alignedRecordSize, t);

		// Find table alignment 
		tableLayout[i].alignedRecordCollectionSize = tableLayout[i].numOfRecords * tableLayout[i].alignedRecordSize;
		t = numeric_limits<size_t>::max();
		std::align(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, tableLayout[i].alignedRecordCollectionSize, (void*&)tableLayout[i].alignedRecordCollectionSize, t);

		totalTableSize += tableLayout[i].alignedRecordCollectionSize;
	}

	// Create the shading table
	// Create host side buffer - it should be zero initialised when using make_unique..
	unique_ptr<uint8_t[]> bufferManager = make_unique<uint8_t[]>(totalTableSize);
	uint8_t* const buffer = bufferManager.get();
	uint8_t* localBuffer = buffer;

	// Extract the properties interface
	Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> pStateObjectProps;
	pStateObject.As(&pStateObjectProps);

	ShadingRecordType shadingRecordType = ShadingRecordType::RayGeneration;
	size_t localRecordNumber = 0;
	// Write each shading record in the buffer
	for (const auto& shadingRecord : shadingRecords) {

		if (shadingRecordType != shadingRecord.shadingRecordType) {
			shadingRecordType = shadingRecord.shadingRecordType;
			localBuffer = buffer + tableLayout[static_cast<int>(shadingRecordType)].offsetInBytes;
			localRecordNumber = 0;
		}

		memcpy(localBuffer, pStateObjectProps->GetShaderIdentifier(shadingRecord.programName.c_str()), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
		localBuffer += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

		// Get parameters
		const auto& rs = rootSignatureManager->getRootSignature(shadingRecord.rootSignatureName);
		for (const auto& parameterName : rs.parameterNames) {
			const auto& parameter = rootSignatureManager->getParameterForRootSignature(shadingRecord.rootSignatureName, parameterName);

			switch (parameter.ParameterType) {
			case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
			{	// Get heap
				const auto d = shadingRecord.managedDescriptorHeapMap.at(parameterName);

				const auto& descriptorHeap = d->getDescriptorHeap();
				//const auto& descriptorHeap = shadingRecord.descriptorHeapMap.at(parameterName);
				UINT64 descriptorTableHandle = descriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr;
				memcpy(localBuffer, &descriptorTableHandle, sizeof(descriptorTableHandle));
			}
			break;
			case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
			{
				throw runtime_error("D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS not implemented in ShadingTable");
			}
			break;
			case D3D12_ROOT_PARAMETER_TYPE_CBV:
			case D3D12_ROOT_PARAMETER_TYPE_SRV:
			case D3D12_ROOT_PARAMETER_TYPE_UAV:
			{
				const auto& view = shadingRecord.viewsMap.at(parameterName);
				auto handle = view.resource->GetGPUVirtualAddress() + view.offsetInBytes;
				memcpy(localBuffer, &handle, sizeof(handle));
			}
			break;
			}

			localBuffer += 8;
		}

		// Calculate next record location
		localBuffer = buffer +
			tableLayout[static_cast<int>(shadingRecord.shadingRecordType)].offsetInBytes +
			tableLayout[static_cast<int>(shadingRecord.shadingRecordType)].alignedRecordSize * ++localRecordNumber;
	}

	// Upload buffer to gpu
	pShadingTable = DXUtil::uploadDataToDefaultHeap(pDevice, pCurrentCommandList, shadingTableTempResource, buffer, totalTableSize, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	pShadingTable->SetName(L"Shading Table");
	return pShadingTable;
}

void ShadingTable::addProgramAssociationsToSubobject(CD3DX12_STATE_OBJECT_DESC& stateObjectDesc)
{
	// build map
	unordered_map<string, vector<wstring>> associations;

	for (const auto& shadingRecord : shadingRecords)
		associations[shadingRecord.rootSignatureName].push_back(shadingRecord.programName);

	vecAssociations.clear();
	vecAssociations.reserve(associations.size()); // DO NOT REMOVE THIS LINE
	for (auto& association : associations)
	{
		CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT& rgAssociation = vecAssociations.emplace_back(stateObjectDesc);
		for (auto& programName : association.second)
			rgAssociation.AddExport(programName.c_str());

		const auto& rs = rootSignatureManager->getRootSignature(association.first);
		rgAssociation.SetSubobjectToAssociate(rs.subobject);
	}
}

D3D12_DISPATCH_RAYS_DESC ShadingTable::getDispatchRaysDescriptor(UINT32 width, UINT32 height) const
{
	D3D12_DISPATCH_RAYS_DESC dispatchRaysDesc = {};

	// Dimensions of the compute grid
	dispatchRaysDesc.Width = width;
	dispatchRaysDesc.Height = height;
	dispatchRaysDesc.Depth = 1;

	// Ray generation shader record
	dispatchRaysDesc.RayGenerationShaderRecord.StartAddress = pShadingTable->GetGPUVirtualAddress() + tableLayout[0].offsetInBytes;
	dispatchRaysDesc.RayGenerationShaderRecord.SizeInBytes = tableLayout[0].numOfRecords * tableLayout[0].alignedRecordSize;

	// Miss ray record
	if (tableLayout[1].numOfRecords > 0)
	{
		dispatchRaysDesc.MissShaderTable.StartAddress = pShadingTable->GetGPUVirtualAddress() + tableLayout[1].offsetInBytes;
		dispatchRaysDesc.MissShaderTable.StrideInBytes = tableLayout[1].alignedRecordSize;
		dispatchRaysDesc.MissShaderTable.SizeInBytes = dispatchRaysDesc.MissShaderTable.StrideInBytes * tableLayout[1].numOfRecords;
	}

	// Hit ray record
	if (tableLayout[2].numOfRecords > 0)
	{
		dispatchRaysDesc.HitGroupTable.StartAddress = pShadingTable->GetGPUVirtualAddress() + tableLayout[2].offsetInBytes;;
		dispatchRaysDesc.HitGroupTable.StrideInBytes = tableLayout[2].alignedRecordSize;
		dispatchRaysDesc.HitGroupTable.SizeInBytes = dispatchRaysDesc.HitGroupTable.StrideInBytes * tableLayout[2].numOfRecords;
	}

	return dispatchRaysDesc;
}

void ShadingTable::validateInputs()
{
	bool rayGenEncountered = false;
	// Check that all parameters are set for every record
	for (const auto& shadingRecord : shadingRecords) {

		if (rayGenEncountered && shadingRecord.shadingRecordType == ShadingRecordType::RayGeneration)
			throw runtime_error("Only one ray gen program is allowed");
		rayGenEncountered = shadingRecord.shadingRecordType == ShadingRecordType::RayGeneration;

		const auto& rs = rootSignatureManager->getRootSignature(shadingRecord.rootSignatureName);
		for (const auto& parameterName : rs.parameterNames) {
			const auto& parameter = rootSignatureManager->getParameterForRootSignature(shadingRecord.rootSignatureName, parameterName);

			string progName = WStringToString(shadingRecord.programName);

			switch (parameter.ParameterType) 
			{
			case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
				if (shadingRecord.managedDescriptorHeapMap.find(parameterName) == shadingRecord.managedDescriptorHeapMap.end())
					throw runtime_error("Empty input for parameter '" + parameterName + "' in program '" + progName + "'");
				shadingRecord.managedDescriptorHeapMap.at(parameterName)->validate();
				break;
			case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
				if (shadingRecord.constantsMap.find(parameterName) == shadingRecord.constantsMap.end())
					throw runtime_error("Empty input for parameter '" + parameterName + "' in program '" + progName + "'");
				break;
			case D3D12_ROOT_PARAMETER_TYPE_CBV:
			case D3D12_ROOT_PARAMETER_TYPE_SRV:
			case D3D12_ROOT_PARAMETER_TYPE_UAV:
				if (shadingRecord.viewsMap.find(parameterName) == shadingRecord.viewsMap.end())
					throw runtime_error("Empty input for parameter '" + parameterName + "' in program '" + progName + "'");
				break;
			}
		}
	}
}

void ShadingTable::sortShadingRecords()
{
	std::stable_sort(shadingRecords.begin(), shadingRecords.end(), [](const ShadingRecord& a, const ShadingRecord& b) -> int {
		return a.shadingRecordType < b.shadingRecordType;
		});

	// Correct the shading record map
	size_t i = 0;
	for (const auto& shadingRecord : shadingRecords) {
		shadingRecordsMap[shadingRecord.programName] = i++;
	}
}

size_t ShadingTable::getShadingRecordSize(const std::wstring& programName)
{
	const auto& shadingRecord = shadingRecords[shadingRecordsMap[programName]];
	const auto& rs = rootSignatureManager->getRootSignature(shadingRecord.rootSignatureName);
	return D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + rs.parameterNames.size() * sizeof(UINT64);
}

size_t ShadingTable::getLargestRecordTypeSize(ShadingRecordType shadingRecordType)
{
	size_t recordSize = 0;
	for (const auto& shadingRecord : shadingRecords) {
		if (shadingRecord.shadingRecordType == shadingRecordType) {
			size_t srSize = getShadingRecordSize(shadingRecord.programName);
			recordSize = max(recordSize, srSize);
		}
	}

	return recordSize;
}

DescriptorHeap::DescriptorHeap(std::shared_ptr<RootSignatureManager> rootSignatureManager, const std::string& parameterName, const std::string& instanceName, Microsoft::WRL::ComPtr<ID3D12Device> pDevice)
	: rootSignatureManager(rootSignatureManager), parameterName(parameterName), instanceName(instanceName)
{
	UINT32 descriptorHeapSize = rootSignatureManager->getDescriptorHeapTotalEntrySize(parameterName);
	descriptorHeap = rootSignatureManager->generateDescriptorHeapForRangeParameter(parameterName, pDevice);
	resources.resize(descriptorHeapSize);
}

void DescriptorHeap::setCBV(size_t entryNumber, const D3D12_CONSTANT_BUFFER_VIEW_DESC& cbvDescriptor, Microsoft::WRL::ComPtr<ID3D12Device> pDevice)
{
	if (rootSignatureManager->getDescriptorHeapRangeType(parameterName, entryNumber) != D3D12_DESCRIPTOR_RANGE_TYPE_CBV)
		throw runtime_error("Entry " + to_string(entryNumber) + " in not of type CBV");
	pDevice->CreateConstantBufferView(&cbvDescriptor, getCpuDescHandle(entryNumber, pDevice));
	setResource(entryNumber);
}

void DescriptorHeap::setUAV(size_t entryNumber, const D3D12_UNORDERED_ACCESS_VIEW_DESC& uavDescriptor, Microsoft::WRL::ComPtr<ID3D12Device> pDevice, Microsoft::WRL::ComPtr<ID3D12Resource> resource)
{
	if (rootSignatureManager->getDescriptorHeapRangeType(parameterName, entryNumber) != D3D12_DESCRIPTOR_RANGE_TYPE_UAV)
		throw runtime_error("Entry " + to_string(entryNumber) + " in not of type UAV");
	pDevice->CreateUnorderedAccessView(resource.Get(), nullptr, &uavDescriptor, getCpuDescHandle(entryNumber, pDevice));
	setResource(entryNumber, resource);
}

void DescriptorHeap::setSRV(size_t entryNumber, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDescriptor, Microsoft::WRL::ComPtr<ID3D12Device> pDevice, Microsoft::WRL::ComPtr<ID3D12Resource> resource)
{
	if (rootSignatureManager->getDescriptorHeapRangeType(parameterName, entryNumber) != D3D12_DESCRIPTOR_RANGE_TYPE_SRV)
		throw runtime_error("Entry " + to_string(entryNumber) + " in not of type SRV");
	pDevice->CreateShaderResourceView(resource.Get(), &srvDescriptor, getCpuDescHandle(entryNumber, pDevice));
	setResource(entryNumber, resource);
}

void DescriptorHeap::validate() const
{
	for (size_t i = 0; i < resources.size(); ++i) {
		if (!resources[i].set)
			throw runtime_error("Entry " + to_string(i) + " of paramter '" + parameterName + "'  of instance '" + instanceName + "' is not set");
	}
}

const string& DescriptorHeap::getInstanceName() const
{
	return instanceName;
}

const shared_ptr<RootSignatureManager>& DescriptorHeap::getRootSignatureManager() const
{
	return rootSignatureManager;
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DescriptorHeap::getDescriptorHeap() const
{
	return descriptorHeap;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::getCpuDescHandle(size_t entryNumber, Microsoft::WRL::ComPtr<ID3D12Device> pDevice) const
{
	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	cpuDescHandle.ptr += entryNumber * pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	return cpuDescHandle;
}

void DescriptorHeap::setResource(size_t entryNumber, Microsoft::WRL::ComPtr<ID3D12Resource> resource)
{
	resources[entryNumber].resource = resource;
	resources[entryNumber].set = true;
}
