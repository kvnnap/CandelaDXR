#pragma once
#define NOMINMAX

#include <d3d12.h>
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>

#include "RootSignatureManager.h"

namespace candela::directx
{
	enum class ShadingRecordType {
		RayGeneration = 0,
		Miss = 1,
		HitGroup = 2
	};

	struct ResourceSet {
		bool set = false;
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
	};

	class DescriptorHeap {
	public:
		DescriptorHeap(std::shared_ptr<RootSignatureManager> rootSignatureManager, const std::string& parameterName, const std::string& instanceName, Microsoft::WRL::ComPtr<ID3D12Device> pDevice);

		void setCBV(size_t entryNumber, const D3D12_CONSTANT_BUFFER_VIEW_DESC& cbvDescriptor, Microsoft::WRL::ComPtr<ID3D12Device> pDevice);
		void setUAV(size_t entryNumber, const D3D12_UNORDERED_ACCESS_VIEW_DESC& uavDescriptor, Microsoft::WRL::ComPtr<ID3D12Device> pDevice, Microsoft::WRL::ComPtr<ID3D12Resource> resource = {});
		void setSRV(size_t entryNumber, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDescriptor, Microsoft::WRL::ComPtr<ID3D12Device> pDevice, const Microsoft::WRL::ComPtr<ID3D12Resource>& resource = {});

		void validate() const;
		const std::string& getInstanceName() const;
		const std::shared_ptr<RootSignatureManager>& getRootSignatureManager() const;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> getDescriptorHeap() const;
	private:
		D3D12_CPU_DESCRIPTOR_HANDLE getCpuDescHandle(size_t entryNumber, Microsoft::WRL::ComPtr<ID3D12Device> pDevice) const;
		void setResource(size_t entryNumber, Microsoft::WRL::ComPtr<ID3D12Resource> resource = {});

		std::shared_ptr<RootSignatureManager> rootSignatureManager;
		std::string parameterName;
		std::string instanceName;

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
		std::vector<ResourceSet> resources;
	};

	struct ViewStruct {
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		UINT64 offsetInBytes = 0;
	};

	struct ShadingRecord {
		std::wstring programName;
		std::string rootSignatureName;
		ShadingRecordType shadingRecordType;

		// paramter values
		std::unordered_map<std::string, UINT32> constantsMap;
		std::unordered_map<std::string, ViewStruct> viewsMap;
		//std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> descriptorHeapMap;
		std::unordered_map<std::string, DescriptorHeap*> managedDescriptorHeapMap;
	};

	struct ShadingTableLayout {
		std::size_t offsetInBytes;
		std::size_t alignedRecordSize;
		std::size_t alignedRecordCollectionSize; // For table
		std::size_t numOfRecords;
	};

	// The shading table will map types (from root signature) to buffers
	class ShadingTable
	{
	public:

		// Keep reference for root signatures
		ShadingTable(std::shared_ptr<RootSignatureManager> rootSignatureManager);

		// 
		void addProgram(const std::wstring& programName, ShadingRecordType shadingRecordType, const std::string& rootSignatureName);

		DescriptorHeap& generateDescriptorHeap(const std::string& parameterName, const std::string& instanceName, Microsoft::WRL::ComPtr<ID3D12Device> pDevice);
		void addDescriptorHeap(std::shared_ptr<DescriptorHeap> descriptorHeap);

		//void setInputForDescriptorTableParameter(const std::wstring& programName, const std::string& parameterName, Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap);
		void setInputForDescriptorTableParameter(const std::wstring& programName, const std::string& parameterName, const std::string& instanceName);
		void setInputForViewParameter(const std::wstring& programName, const std::string& parameterName, Microsoft::WRL::ComPtr<ID3D12Resource> resource, UINT64 offsetInBytes = 0);
		void setInputForConstantParameter(const std::wstring& programName, const std::string& parameterName, UINT32 constant);

		Microsoft::WRL::ComPtr<ID3D12Resource> generateShadingTable(
			Microsoft::WRL::ComPtr<ID3D12Device> pDevice,
			Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList,
			Microsoft::WRL::ComPtr<ID3D12StateObject> pStateObject,
			Microsoft::WRL::ComPtr<ID3D12Resource>& shadingTableTempResource);

		void addProgramAssociationsToSubobject(CD3DX12_STATE_OBJECT_DESC& stateObjectDesc);

		D3D12_DISPATCH_RAYS_DESC getDispatchRaysDescriptor(UINT32 width, UINT32 height) const;

	private:
		void validateInputs();
		void sortShadingRecords();
		size_t getShadingRecordSize(const std::wstring& programName);
		size_t getLargestRecordTypeSize(ShadingRecordType shadingRecordType);

		std::shared_ptr<RootSignatureManager> rootSignatureManager;
		std::unordered_map<std::wstring, size_t> shadingRecordsMap;
		std::vector<ShadingRecord> shadingRecords;

		std::unordered_map<std::string, std::shared_ptr<DescriptorHeap>> descriptorHeaps;

		ShadingTableLayout tableLayout[3];

		std::vector<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT> vecAssociations;

		Microsoft::WRL::ComPtr<ID3D12Resource> pShadingTable;
	};
}
