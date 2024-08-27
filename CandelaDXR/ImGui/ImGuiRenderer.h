#pragma once

#include "feanor/anvil/core/anvil.h"

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

		// PP stuff
		bool exposureFlag;
		bool linearToSrgb;
		float exposure;
		int selectedToneMapper;

		bool changed;
		bool animating;
		bool shaderAccumulation;
		bool showLights;

		int timeMs;

		ANVIL_CODE_RAW(
			bool anvilEnabled;
		)
	};
}
