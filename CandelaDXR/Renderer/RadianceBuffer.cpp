#include "RadianceBuffer.h"

using candela::renderer::RadianceBuffer;
using candela::renderer::RgbSpectrum;

using std::size_t;

RadianceBuffer::RadianceBuffer()
    : width(), height()
{
}

RadianceBuffer::RadianceBuffer(directx::ResourceData&& resourceData)
    : width (resourceData.Width), height (resourceData.Height), radiance (std::move(resourceData.data))
{
}

void RadianceBuffer::reset(std::size_t p_width, std::size_t p_height)
{
    width = p_width;
    height = p_height;
    radiance = std::vector<RgbSpectrum>(width * height);
}

std::size_t RadianceBuffer::getWidth() const
{
    return width;
}

std::size_t RadianceBuffer::getHeight() const
{
    return height;
}

size_t RadianceBuffer::getIndex(std::size_t x, std::size_t y) const
{
    return y * width + x;
}

const RgbSpectrum& RadianceBuffer::get(std::size_t x, std::size_t y) const
{
    return radiance[getIndex(x, y)];
}

RgbSpectrum& RadianceBuffer::get(std::size_t x, std::size_t y)
{
    return radiance[getIndex(x, y)];
}

const std::vector<RgbSpectrum>& RadianceBuffer::getInternalBuffer() const
{
    return radiance;
}

std::vector<RgbSpectrum>& RadianceBuffer::getInternalBuffer()
{
    return radiance;
}

