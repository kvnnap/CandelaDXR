#pragma once

#include "IChain.h"

namespace candela::chain
{
    class ToneMapping
        : public IChain
    {
    public:
        void process(renderer::RadianceBuffer& buffer) override;
    };
}
