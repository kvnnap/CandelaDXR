#include "TextureFactory.h"

#include "Exception/Exception.h"

using std::unique_ptr;
using std::make_unique;

using feanor::configuration::ConfigurationNode;

using candela::scene::factory::TextureFactory;
using candela::scene::Texture;
using candela::scene::StbTexture;
using candela::scene::MemoryTexture;

unique_ptr<Texture> TextureFactory::create(const std::string& fileName) const
{
	auto tex = make_unique<StbTexture>(fileName);
	if (tex->loaded())
		return tex;
	ThrowException("Texture '" + fileName + "' cannot be loaded");
}

unique_ptr<Texture> TextureFactory::create(const void* imageData, std::size_t len) const
{
	auto tex = make_unique<StbTexture>(imageData, len);
	if (tex->loaded())
		return tex;
	ThrowException("Texture cannot be loaded");
}

unique_ptr<Texture> TextureFactory::createRaw(const void* imageData, int width, int height, int bytesPerPixel) const
{
	return make_unique<MemoryTexture>(imageData, width, height, bytesPerPixel);
}
