#pragma once

#include "feanor/core/factory/factory.h"
#include "Scene/Scene.h"

namespace candela::scene::factory
{
    class TextureFactory
    {
    public:
        virtual ~TextureFactory() = default;

        std::unique_ptr<Texture> create(const std::string& fileName) const;
        std::unique_ptr<Texture> create(const void* imageData, std::size_t len) const;
        std::unique_ptr<Texture> createRaw(const void* imageData, int width, int height, int bytesPerPixel = 4) const;
    };
}