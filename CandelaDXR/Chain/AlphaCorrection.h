#pragma once

#include "IChain.h"

namespace candela::chain
{
    class AlphaCorrection
        : public IChain
    {
    public:
        AlphaCorrection();

        void process(renderer::RadianceBuffer& buffer) override;

        void setGamma(float gamma);

    private:
        float gamma;
    };
}
