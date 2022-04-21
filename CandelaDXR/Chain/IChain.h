#pragma once

#include "Renderer/RadianceBuffer.h"

namespace candela::chain
{
    class IChain
    {
    public:
        virtual ~IChain() = default;
        virtual void process(renderer::RadianceBuffer& buffer) = 0;
    };
}