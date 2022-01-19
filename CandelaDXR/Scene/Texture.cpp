#include "stb/stb_image.h"

#include "Texture.h"

using candela::scene::Texture;

Texture::Texture(const std::string& fileName)
	: dataBuffer(nullptr, stbImageDeleter), width(), height(), channels()
{
	int imageChannels;
	dataBuffer = StbImagePtr(stbi_load(fileName.c_str(), &width, &height, &imageChannels, channels = STBI_rgb_alpha), stbImageDeleter);
}

void Texture::stbImageDeleter(unsigned char* image)
{
	if (image != nullptr)
		stbi_image_free(image);
}

const unsigned char* Texture::data() const
{
	return dataBuffer.get();
}

int Texture::getWidth() const
{
	return width;
}

int Texture::getHeight() const
{
	return height;
}

int Texture::getChannels() const
{
	return channels;
}

int Texture::getBytesPerPixel() const
{
	return channels * 8;
}
