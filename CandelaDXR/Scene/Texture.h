#pragma once

#include <cstdint>
#include <string>
#include <memory>

namespace candela::scene
{
	class Texture
	{
	public:
		Texture(const std::string& fileName);

		static void stbImageDeleter(unsigned char* image);
		using StbImagePtr = std::unique_ptr<unsigned char, decltype(&stbImageDeleter)>;

		const unsigned char* data() const;
		int getWidth() const;
		int getHeight() const;
		int getChannels() const;
		int getBytesPerPixel() const;

	private:

		StbImagePtr dataBuffer;
		int width;
		int height;
		int channels;
	};
}
