#include "RadianceBuffer.h"

#include "Util/StringUtil.h"

using candela::renderer::RadianceBuffer;
using candela::renderer::RgbSpectrum;

using candela::util::WStringToString;

using std::size_t;

RadianceBuffer::RadianceBuffer()
    : width(), height()
{
}

RadianceBuffer::RadianceBuffer(directx::ResourceData&& resourceData)
    : name(WStringToString(resourceData.Name)), width (resourceData.Width), height (resourceData.Height)
{
    radiance.reserve(width * height);
    for (auto& datum : resourceData.data)
        radiance.emplace_back(RgbSpectrum{ datum.x, datum.y, datum.z });
    resourceData.data = {};
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

const std::string& RadianceBuffer::getName() const
{
    return name;
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

