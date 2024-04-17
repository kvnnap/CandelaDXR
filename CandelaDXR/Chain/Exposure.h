#pragma once

#include "IChain.h"

namespace candela::chain
{
    class Exposure
        : public IChain
    {
    public:
        Exposure();

        void process(renderer::RadianceBuffer& buffer) override;

        void setExposure(float exposure);

    private:
        float exposure;
    };
}
