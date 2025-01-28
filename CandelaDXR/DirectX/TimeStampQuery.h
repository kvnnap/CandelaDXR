#pragma once

#include <cstddef>
#include <vector>
#include <string>

#include "Resource.h"
#include "ResourceManager.h"

namespace candela::directx
{
	struct ProfileItem
	{
		std::string ProfileName;
		float TimeMs;
	};

	class TimeStampQuery
	{
	public:
		TimeStampQuery();

		void init(DXDevice pDevice, ResourceManager *resourceManager, DXCommandQueue& commandQueue);
		void addTimeStampQuery(DXCommandList pCommandList, const std::string& profName);
		const std::vector<ProfileItem>& load();
		const std::vector<ProfileItem>& getLoadedItems() const;
		void resolve(DXCommandList pCommandList);
	private:
		static constexpr auto numTimeStamps = 1024U;
		wrl::ComPtr<ID3D12QueryHeap> queryHeap;
		Resource* queryResource;
		double frequency;
		std::vector<std::string> committedProfNames;
		std::vector<std::string> profNames;
		std::vector<ProfileItem> profItems;
	};
}
