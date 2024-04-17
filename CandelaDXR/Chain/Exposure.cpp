#include "Exposure.h"
#include "Mathematics/Utils.h"

#include <cmath>

using std::pow;
using namespace DirectX;

using candela::chain::Exposure;
using candela::renderer::RadianceBuffer;
using candela::mathematics::CreateXMVector;

Exposure::Exposure()
    : exposure()
{
}

void Exposure::process(RadianceBuffer& radianceBuffer)
{
    const auto factor = CreateXMVector(pow(2.f, exposure));

    for (auto& abc_ : radianceBuffer.getInternalBuffer())
        XMStoreFloat3(&abc_, XMLoadFloat3(&abc_) * factor);
}

void Exposure::setExposure(float p_exposure)
{
    exposure = p_exposure;
}
