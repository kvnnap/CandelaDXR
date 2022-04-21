#include "ToneMapping.h"

using candela::chain::ToneMapping;
using candela::renderer::RadianceBuffer;

void ToneMapping::process(RadianceBuffer& radianceBuffer)
{
    for (auto& rgbSpectrum : radianceBuffer.getInternalBuffer())
    {
        rgbSpectrum.x /= rgbSpectrum.x + 1.f;
        rgbSpectrum.y /= rgbSpectrum.y + 1.f;
        rgbSpectrum.z /= rgbSpectrum.z + 1.f;
    }
}
