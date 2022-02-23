#pragma once

#define NOMINMAX

#include <d3d12.h>
#include <wrl/client.h>

#include <string>
#include <unordered_map>

#include "d3dx12.h"


namespace candela::directx
{
	struct RootSignature {
		std::vector<std::string> parameterNames;
		D3D12_STATIC_SAMPLER_DESC sampler;

		// Compiled stuff
		Microsoft::WRL::ComPtr<ID3D12RootSignature> compiledSignature;
		CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT subobject;

		bool samplerSet;

		RootSignature() : sampler{}, subobject{}, samplerSet(false) {};
	};

	class RootSignatureManager
	{
	public:
		// Ranges
		void addDescriptorRange(const std::string& destRangeName, const D3D12_DESCRIPTOR_RANGE1& range);
		void addDescriptorRange(const std::string& destRangeName, const std::string& sourceRangeName);

		// Root signature Params - These will override any previous values if name is the same
		void setDescriptorTableParameter(const std::string& destParameterName, const std::string& descriptorRangeName);
		void setParameter(const std::string& destParameterName, const D3D12_ROOT_PARAMETER1& parameter);

		// Create signature (from params)
		void addRootSignature(const std::string& rootSignatureName);
		void setSamplerForRootSignature(const std::string& rootSignatureName, const D3D12_STATIC_SAMPLER_DESC& sampler);
		void addParameterToRootSignature(const std::string& destSignatureName, const std::string& parameterName);
		void addParametersToRootSignature(const std::string& destSignatureName, const std::vector<std::string>& parameterNames);

		Microsoft::WRL::ComPtr<ID3D12RootSignature> generateRootSignature(const std::string& rootSigatureName, Microsoft::WRL::ComPtr<ID3D12Device> pDevice);
		UINT32 getDescriptorHeapTotalEntrySize(const std::string& parameterName) const;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> generateDescriptorHeapForRangeParameter(const std::string& parameterName, Microsoft::WRL::ComPtr<ID3D12Device> pDevice) const;
		D3D12_DESCRIPTOR_RANGE_TYPE getDescriptorHeapRangeType(const std::string& parameterName, size_t entryNumber) const;
		void addRootSignaturesToSubObject(CD3DX12_STATE_OBJECT_DESC& stateObjectDesc);

		const std::vector<D3D12_DESCRIPTOR_RANGE1>& getDescriptorRanges(const std::string& descriptorRangeName) const;
		const D3D12_ROOT_PARAMETER1& getParameter(const std::string& parameterName) const;
		const D3D12_ROOT_PARAMETER1& getParameterForRootSignature(const std::string& rootSignatureName, const std::string& parameterName) const;
		const RootSignature& getRootSignature(const std::string& rootSignatureName) const;

	private:
		void validateDescriptorHeap(const std::string& parameterName) const;

		std::unordered_map<std::string, std::vector<D3D12_DESCRIPTOR_RANGE1>> descriptorRanges;
		std::unordered_map<std::string, D3D12_ROOT_PARAMETER1> parameters;

		//D3D12_VERSIONED_ROOT_SIGNATURE_DESC
		std::unordered_map<std::string, RootSignature> rootSignatures;
	};
}
