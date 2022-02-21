#pragma once

namespace candela::sampler
{
    template<typename T>
    class UniformRealDistribution
    {
    public:
        constexpr UniformRealDistribution(T min, T max) : min(min), max(max) {}

        template <class E>
        float operator()(E& e) const
        {
            constexpr T range = static_cast<T>(E::max()) - static_cast<T>(E::min())/* + T{1}*/;
            return (e() / range) * (max - min) + min;
        }
    private:
        const T min, max;
    };
}
