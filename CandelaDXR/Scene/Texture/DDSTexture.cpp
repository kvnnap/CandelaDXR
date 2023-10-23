#include "DDSTexture.h"

#include "Exception/Exception.h"

using candela::scene::DDSTexture;

DDSTexture::DDSTexture(const std::string& fileName)
{
	result = dds::readFile(fileName, &image);
	if (result == dds::ReadResult::Success)
	{
		width = image.width;
		height = image.height;
		bitsPerPixel = (image.mipmaps.front().size() * 8) / (image.width * image.height);
		if (image.dimension != dds::ResourceDimension::Texture2D)
			ThrowException("DDS Image dimension not 2D");
	}
}

const unsigned char* candela::scene::DDSTexture::data() const
{
	return image.mipmaps.front().data();
}

bool candela::scene::DDSTexture::loaded() const
{
	return result == dds::ReadResult::Success;
}

DXGI_FORMAT DDSTexture::getTextureDXGIFormat() const
{
	return image.format;
}
