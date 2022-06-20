#pragma once

#include <cstdint>
#include <type_traits>

namespace candela::renderer
{
	enum class ChangeEvent : std::uint32_t
	{
		Transformation = 0x01,	// Like Matrix Rotation, etc
		SceneUpdate = 0x02,		// Scene buffer content change but no size change
		SceneChange = 0x04,		// Scene buffer size change (like num of lights, specs and so on)
		Statistics = 0x08,		// Update stats only
		Camera = 0x10			// Camera updated
	};

	using ChangeEvent_t = std::underlying_type<ChangeEvent>::type;
}
