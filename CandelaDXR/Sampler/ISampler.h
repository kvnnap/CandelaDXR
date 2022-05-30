#pragma once

#include <stddef.h>
#include <vector>
#include <cstdint>

namespace candela::sampler
{
    class ISampler {
    public:
        virtual ~ISampler() = default;

        // Generates [0, 1]
        virtual float nextSample() = 0;

        // Generates [min, max]
        virtual float nextSample(float min, float max) = 0;

        // Generates samples where each elem is [0, 1]
        virtual std::vector<float> nextSamples(size_t p_numSamples) = 0;

        // Generates a uint32 random sample
        virtual std::uint32_t nextUInt32() = 0;

        virtual std::uint32_t getSeed() const = 0;

        virtual std::size_t chooseInRange(size_t a, size_t b) = 0;
    };
}

