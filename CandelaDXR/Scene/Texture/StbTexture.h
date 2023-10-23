#pragma once

#include "Texture.h"

namespace candela::scene
{
	class StbTexture
		: public Texture
	{
	public:
		StbTexture(const std::string& fileName);
		StbTexture(const void* imageData, std::size_t len);

		const unsigned char* data() const override;
		bool loaded() const override;

		static void stbImageDeleter(unsigned char* image);
		using StbImagePtr = std::unique_ptr<unsigned char, decltype(&stbImageDeleter)>;

	private:
		StbImagePtr dataBuffer;
	};
}
