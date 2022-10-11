#pragma once

namespace candela::renderer
{
	class Renderer;
}

namespace candela::renderer::imgui
{
	class ImGuiRenderer
	{
	public:
		ImGuiRenderer(Renderer& renderer);

		void drawUi();

		bool hasChanged() const;
	private:
		Renderer& renderer;

		bool changed;
		bool animating;
		bool shaderAccumulation;

		int timeMs;
	};
}
