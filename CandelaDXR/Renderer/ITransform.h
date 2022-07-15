#pragma once

#include "Mathematics/Types.h"

namespace candela::renderer
{
	class ITransform
	{
	public:
		virtual ~ITransform() = default;

		virtual void transform(const mathematics::Matrix& trans) = 0;

		virtual const mathematics::Vector& getCentrePosition() const = 0;
	};
}
