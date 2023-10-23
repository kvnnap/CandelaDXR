#include "MemoryTexture.h"

using candela::scene::MemoryTexture;

MemoryTexture::MemoryTexture(const void* imageData, int width, int height, int bytesPerPixel)
{
	auto sizeInBytes = width * height * bytesPerPixel;
	dataBuffer = std::make_unique<unsigned char[]>(sizeInBytes);
	memcpy(dataBuffer.get(), imageData, sizeInBytes);
	this->width = width;
	this->height = height;
	this->bitsPerPixel = bytesPerPixel * 8;
}

const unsigned char* MemoryTexture::data() const
{
	return dataBuffer.get();
}

bool MemoryTexture::loaded() const
{
	return !!dataBuffer;
}
