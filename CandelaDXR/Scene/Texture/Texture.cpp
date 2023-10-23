
#include "Texture.h"

using candela::scene::Texture;

Texture::Texture()
	: width(), height(), bitsPerPixel()
{
}

DXGI_FORMAT Texture::getTextureDXGIFormat() const
{
	return DXGI_FORMAT_R8G8B8A8_UNORM;
}

int Texture::getWidth() const
{
	return width;
}

int Texture::getHeight() const
{
	return height;
}

int Texture::getBitsPerPixel() const
{
	return bitsPerPixel;
}
