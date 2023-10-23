#pragma once

#include "Texture.h"

namespace candela::scene
{
	class MemoryTexture
		: public Texture
	{
	public:
		MemoryTexture(const void* imageData, int width, int height, int bytesPerPixel = 4);

		const unsigned char* data() const override;
		bool loaded() const override;

	private:
		std::unique_ptr<unsigned char[]> dataBuffer;
	};
}
