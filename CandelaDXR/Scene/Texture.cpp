#include "stb/stb_image.h"

#include "Texture.h"

#include "Exception/Exception.h"

using candela::scene::Texture;
using candela::scene::MemoryTexture;
using candela::scene::StbTexture;

Texture::Texture()
	: width(), height(), channels()
{
}

MemoryTexture::MemoryTexture(const void* imageData, int width, int height, int bytesPerPixel)
{
	auto sizeInBytes = width * height * bytesPerPixel;
	dataBuffer = std::make_unique<unsigned char[]>(sizeInBytes);
	memcpy(dataBuffer.get(), imageData, sizeInBytes);
	this->width = width;
	this->height = height;
	this->channels = bytesPerPixel;
}

const unsigned char* MemoryTexture::data() const
{
	return dataBuffer.get();
}

StbTexture::StbTexture(const std::string& fileName)
	: dataBuffer(nullptr, stbImageDeleter)
{
	int imageChannels;
	dataBuffer = StbImagePtr(stbi_load(fileName.c_str(), &width, &height, &imageChannels, channels = STBI_rgb_alpha), stbImageDeleter);
	if (!dataBuffer)
		ThrowException("Texture '" + fileName + "' cannot be loaded");
}

StbTexture::StbTexture(const void* imageData, std::size_t len)
	: dataBuffer(nullptr, stbImageDeleter)
{
	int imageChannels;
	dataBuffer = StbImagePtr(stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(imageData), static_cast<int>(len), &width, &height, &imageChannels, channels = STBI_rgb_alpha), stbImageDeleter);
	if (!dataBuffer)
		ThrowException("Texture cannot be loaded");
}

const unsigned char* StbTexture::data() const
{
	return dataBuffer.get();
}

void StbTexture::stbImageDeleter(unsigned char* image)
{
	if (image != nullptr)
		stbi_image_free(image);
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


