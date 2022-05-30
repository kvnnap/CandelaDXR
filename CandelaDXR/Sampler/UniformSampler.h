#pragma once

#include "isampler.h"

#include <cstdint>

#include <random>

#include "UniformRealDistribution.h"

namespace candela::sampler
{
    class UniformSampler
        : public ISampler
    {
    public:
        UniformSampler();
        UniformSampler(std::uint32_t p_seed);

        // Generates [0, 1]
        float nextSample() override;

        // Generates [min, max]
        float nextSample(float min, float max) override;

        // Generates a uint32 random sample
        std::uint32_t nextUInt32() override;

        // Generates samples where each elem is [0, 1]
        std::vector<float> nextSamples(size_t p_numSamples) override;
        std::uint32_t getSeed() const override;

        // Generates integer [min, max]
        std::size_t chooseInRange(size_t a, size_t b) override;
    private:
        std::uint32_t seed;
        std::mt19937 mt;
        UniformRealDistribution<float> dist;
    };
}
