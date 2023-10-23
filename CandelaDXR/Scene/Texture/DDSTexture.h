#pragma once

#include "Texture.h"

#include <span>
#include "dds/dds.hpp"

namespace candela::scene
{
	class DDSTexture
		: public Texture
	{
	public:
		DDSTexture(const std::string& fileName);
		//DDSTexture(const void* imageData, std::size_t len);

		const unsigned char* data() const override;
		bool loaded() const override;
		DXGI_FORMAT getTextureDXGIFormat() const override;

	private:
		dds::ReadResult result{};
		dds::Image image{};
	};
}
