#pragma once

#include <cstdint>
#include <string>
#include <memory>

#include <dxgiformat.h>

namespace candela::scene
{
	class Texture
	{
	public:
		Texture();
		virtual ~Texture() = default;

		virtual const unsigned char* data() const = 0;
		virtual bool loaded() const = 0;
		virtual DXGI_FORMAT getTextureDXGIFormat() const;

		int getWidth() const;
		int getHeight() const;
		int getBitsPerPixel() const;

	protected:
		int width;
		int height;
		int bitsPerPixel;
	};
}
