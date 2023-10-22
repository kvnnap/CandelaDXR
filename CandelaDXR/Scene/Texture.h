#pragma once

#include <cstdint>
#include <string>
#include <memory>

namespace candela::scene
{
	class Texture
	{
	public:
		Texture();
		virtual ~Texture() = default;

		virtual const unsigned char* data() const = 0;
		virtual bool loaded() const = 0;

		int getWidth() const;
		int getHeight() const;
		int getChannels() const;
		int getBytesPerPixel() const;

	protected:
		int width;
		int height;
		int channels;
	};

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
