#pragma once

#include <cstdint>

#include "Mathematics/Types.h"

namespace candela::scene
{
	struct alignas(16) Material
	{
		mathematics::Vector3 Diffuse; 
		std::int32_t DiffuseTextureId;
		mathematics::Vector3 Emissive;
		std::int32_t EmissiveTextureId;
	};
}

