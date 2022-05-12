#pragma once

#include <limits>

namespace candela::mathematics::constants
{
    // Math
    constexpr float Pi = 3.14159265358979323846f;
    constexpr float TwoPi = Pi * 2;
    constexpr float PiOver2 = Pi / 2;
    constexpr float PiOver4 = Pi / 4;
    constexpr float OneOverPi = 1 / Pi;
    constexpr float TwoOverPi = 2 / Pi;
    constexpr float E = 2.71828182845904523536f;
    constexpr float Ln2 = 0.693147180559945309417f;
    constexpr float Ln10 = 2.30258509299404568402f;
    constexpr float OneOverLn2 = 1 / Ln2;
    constexpr float OneOverLn10 = 1 / Ln10;
    constexpr float FourPi = 4 * Pi;
    constexpr float OneOverFourPi = 1 / (4 * Pi);
    constexpr float OneOverTwoPi = 1 / (2 * Pi);

    // Float
    constexpr float Maximum = std::numeric_limits<float>::max();
    constexpr float Minimum = std::numeric_limits<float>::min();
    constexpr float Infinity = std::numeric_limits<float>::infinity();
    constexpr float NegativeInfinity = -Infinity;
    constexpr float MachineEpsilon = std::numeric_limits<float>::epsilon();
    constexpr float MaxRelativeMachineEpsilon = 1.f / (1.f + MachineEpsilon);
    constexpr float MinRelativeMachineEpsilon = 1.f / (1.f - MachineEpsilon);
    constexpr float StepAfterZero = 1.4012984643248170709237295832899e-45f;
}
