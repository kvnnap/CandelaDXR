#pragma once

#include "Types.h"

namespace candela::mathematics
{
    struct TransformComponents
    {
        Vector Translate;
        Vector Scale;
        Vector Rotate;

        Matrix transform(const Vector& origin = {}, bool translationAbsolute = false) const;
        void setFromMatrix(const Matrix& matrix);
    };
}