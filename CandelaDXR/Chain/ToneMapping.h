#pragma once

#include "IChain.h"

namespace candela::chain
{
    class ToneMapping
        : public IChain
    {
    public:
        enum ToneMappingType
        {
            Reinhard,
            ACES
        };

        ToneMapping(ToneMappingType p_type = ToneMappingType::Reinhard);
        void process(renderer::RadianceBuffer& buffer) override;

    private:
        const ToneMappingType type;
    };
}
