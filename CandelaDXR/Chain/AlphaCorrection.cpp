#include "AlphaCorrection.h"

#include <cmath>

using candela::chain::AlphaCorrection;
using candela::renderer::RadianceBuffer;
using std::pow;

AlphaCorrection::AlphaCorrection()
    : gamma(2.4f)
{
}

// ASSUMPTION: Tone mapper needs to be called before alpha correction
void AlphaCorrection::process(RadianceBuffer& radianceBuffer)
{
    const float encodingGamma = 1.f / gamma;
    
    for (auto& abc_ : radianceBuffer.getInternalBuffer())
    {
        abc_.x = abc_.x <= 0.0031308f ? abc_.x * 12.92f : 1.055f * pow(abc_.x, encodingGamma) - 0.055f;
        abc_.y = abc_.y <= 0.0031308f ? abc_.y * 12.92f : 1.055f * pow(abc_.y, encodingGamma) - 0.055f;
        abc_.z = abc_.z <= 0.0031308f ? abc_.z * 12.92f : 1.055f * pow(abc_.z, encodingGamma) - 0.055f;
    }
}

void AlphaCorrection::setGamma(float p_gamma)
{
    gamma = p_gamma;
}
