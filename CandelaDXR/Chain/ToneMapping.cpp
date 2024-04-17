#include "ToneMapping.h"
#include "Mathematics/Utils.h"

using namespace DirectX;

using candela::chain::ToneMapping;
using candela::renderer::RadianceBuffer;
using candela::mathematics::CreateXMVector;

ToneMapping::ToneMapping(ToneMappingType p_type)
    : type (p_type)
{
}

void ToneMapping::process(RadianceBuffer& radianceBuffer)
{
    if (type == ToneMappingType::Reinhard)
    {
        constexpr auto one = CreateXMVector(1.f);
        
        for (auto& rgbSpectrum : radianceBuffer.getInternalBuffer())
        {
            const auto x = XMLoadFloat3(&rgbSpectrum);
            XMStoreFloat3(&rgbSpectrum, x / (x + one));
        }
    }
    else if (type == ToneMappingType::ACES)
    {
        constexpr auto a = CreateXMVector(2.51f);
        constexpr auto b = CreateXMVector(0.03f);
        constexpr auto c = CreateXMVector(2.43f);
        constexpr auto d = CreateXMVector(0.59f);
        constexpr auto e = CreateXMVector(0.14f);
        constexpr auto s = CreateXMVector(0.6f);

        for (auto& rgbSpectrum : radianceBuffer.getInternalBuffer())
        {
            const auto x = XMLoadFloat3(&rgbSpectrum) * s;
            const auto t1 = XMVectorMultiplyAdd(a, x, b);
            const auto t2 = XMVectorMultiplyAdd(c, x, d);
            const auto den = XMVectorMultiplyAdd(x, t2, e);
            XMStoreFloat3(&rgbSpectrum, XMVectorSaturate((x * t1) / den));
        }
    }
}
