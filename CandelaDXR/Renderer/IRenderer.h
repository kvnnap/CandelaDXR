#pragma once

namespace candela::renderer
{
	class IRenderer
	{
	public:
		virtual ~IRenderer() = default;

		virtual void renderFrame() = 0;
	};
}