//
// Created by kvnna on 22/06/2016.
//

#include <array>
#include <chrono>

#include "UniformSampler.h"

using candela::sampler::UniformSampler;
using candela::sampler::UniformRealDistribution;

using std::uint32_t;
using std::array;
using std::seed_seq;
using std::uintptr_t;
using std::chrono::system_clock;
using std::uniform_int_distribution;
using std::vector;

UniformSampler::UniformSampler(uint32_t p_seed)
    : seed ( p_seed ),
      mt ( p_seed ),
      dist (0.f, 1.f)
{}

UniformSampler::UniformSampler()
    : UniformSampler(
        ([](uint32_t x, uint32_t y) -> uint32_t {
            seed_seq seq{x, y};
            array<uint32_t, 1> seeds;
            seq.generate(seeds.begin(), seeds.end());
            return seeds[0];
        })(static_cast<uint32_t>(system_clock::now().time_since_epoch().count()), static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this)))
    )
{ }

float UniformSampler::nextSample()
{
    return dist(mt);
}

float UniformSampler::nextSample(float min, float max)
{
    return UniformRealDistribution(min, max)(mt);
}

uint32_t UniformSampler::nextUInt32()
{
    return uniform_int_distribution<uint32_t>()(mt);
}

vector<float> UniformSampler::nextSamples(size_t p_numSamples)
{
    vector<float> samples;
    for (size_t i = 0; i < p_numSamples; ++i)
        samples.push_back(nextSample());
    
    return samples;
}

std::size_t UniformSampler::chooseInRange(size_t a, size_t b)
{
    return uniform_int_distribution<size_t>(a, b)(mt);
}

std::uint32_t UniformSampler::getSeed() const
{
    return seed;
}
