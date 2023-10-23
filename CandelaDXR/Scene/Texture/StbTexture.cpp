#include "StbTexture.h"

#include "stb/stb_image.h"

using candela::scene::StbTexture;

StbTexture::StbTexture(const std::string& fileName)
	: dataBuffer(nullptr, stbImageDeleter)
{
	int imageChannels;
	dataBuffer = StbImagePtr(stbi_load(fileName.c_str(), &width, &height, &imageChannels, STBI_rgb_alpha), stbImageDeleter);
	bitsPerPixel = 32;
}

StbTexture::StbTexture(const void* imageData, std::size_t len)
	: dataBuffer(nullptr, stbImageDeleter)
{
	int imageChannels;
	dataBuffer = StbImagePtr(stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(imageData), static_cast<int>(len), &width, &height, &imageChannels, STBI_rgb_alpha), stbImageDeleter);
	bitsPerPixel = 32;
}

const unsigned char* StbTexture::data() const
{
	return dataBuffer.get();
}

bool StbTexture::loaded() const
{
	return !!dataBuffer;
}

void StbTexture::stbImageDeleter(unsigned char* image)
{
	if (image != nullptr)
		stbi_image_free(image);
}
