#pragma once

#include <vector>
#include <memory>

#include "DirectX/Types.h"
#include "DirectX/Resource.h"
#include "Scene/Scene.h"
#include "Mathematics/Types.h"
#include "Camera.h"

namespace candela::renderer
{
	namespace wrl = Microsoft::WRL;

	class AccelerationStructure;

	struct RendererResources
	{
		wrl::ComPtr<ID3D12Device> pDevice;
		wrl::ComPtr<ID3D12Resource> sceneBuffer;
		wrl::ComPtr<ID3D12Resource> materialBuffer;
		wrl::ComPtr<ID3D12Resource> faceAttributeBuffer;
		wrl::ComPtr<ID3D12Resource> lightBuffer;
		wrl::ComPtr<ID3D12Resource> specularBuffer;
		wrl::ComPtr<ID3D12Resource> matrices;
		wrl::ComPtr<ID3D12Resource> normalMatrices;
		std::vector<directx::Resource> textures;
		wrl::ComPtr<ID3D12DescriptorHeap> pRTVDescriptorHeap;
		std::vector<std::shared_ptr<directx::Resource>> pRTVRadBackBuffers;
		directx::CommandQueue* commandQueue;
		mathematics::UVector2 winDimensions;
		UINT numBackBuffers;
		scene::Scene* scene;
		Camera* camera;
		AccelerationStructure* accelerationStructure;
		std::vector<wrl::ComPtr<ID3D12Resource>> initTempBuffers;
		std::vector<std::vector<wrl::ComPtr<ID3D12Resource>>> tempBuffers;
		UINT currentBackBufferIndex;

		directx::DXResource& getTempResource()
		{
			return tempBuffers[currentBackBufferIndex].emplace_back();
		}
	};
}