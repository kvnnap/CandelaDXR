#pragma once

#include <cstdint>

#include "Mathematics/Types.h"

namespace candela::scene
{
	class Material
	{
	public:
		mathematics::Vector4 Diffuse;
		mathematics::Vector4 Emissive;
		std::int32_t DiffuseTextureId;
		std::int32_t EmissiveTextureId;
	};
}

