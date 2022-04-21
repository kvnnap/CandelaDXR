#pragma once

#include <cstddef>
#include <vector>

#include <DirectXMath.h>

#include "DirectX/Resource.h"

namespace candela::renderer
{
    using RgbSpectrum = DirectX::XMFLOAT4;
    class RadianceBuffer
    {
    public:
        RadianceBuffer();
        RadianceBuffer(directx::ResourceData && resourceData);

        void reset(std::size_t width, std::size_t height);

        std::size_t getWidth() const;
        std::size_t getHeight() const;

        const RgbSpectrum& get(std::size_t x, std::size_t y) const;
        RgbSpectrum& get(std::size_t x, std::size_t y);
        const std::vector<RgbSpectrum>& getInternalBuffer() const;
        std::vector<RgbSpectrum>& getInternalBuffer();
    private:
        size_t getIndex(std::size_t x, std::size_t y) const;

        std::size_t width;
        std::size_t height;
        std::vector<RgbSpectrum> radiance;
    };
}
