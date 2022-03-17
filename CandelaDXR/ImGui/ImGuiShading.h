#pragma once

#include "Renderer/IDrawable.h"

#include "Mathematics/Types.h"

namespace candela::renderer::imgui
{
	class ImGuiShading
		: public IVisitor
	{
	public:
		ImGuiShading(IDrawable *drawable);

		void drawUi();

		bool hasChanged() const;
		bool isEnabled() const;

		void visit(RasterShading*) override;
		void visit(LightTracingShading*) override;

	private:
		IDrawable *drawable;
		bool initialised;
		bool changed;
		bool enabled;
		union {
			// Raster Shading
			struct {
				
			};

			// Light Shading
			struct {
				int lightSamples[2];
			};
		};
	};
}