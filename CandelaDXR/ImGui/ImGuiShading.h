#pragma once

#include <vector>
#include <string>

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
		void visit(PathTracingShading*) override;

	private:
		enum PathInteraction : std::uint32_t
		{
			Light = 1,
			Reflect = 2,
			Refract = 4,
			Diffuse = 8
		};

		static bool processPathFilter(uint32_t &pathFlags, bool (&cPathFlags)[4]);

		IDrawable *drawable;
		bool initialised;
		bool changed;
		bool enabled;

		std::vector<std::string> shaderStrNames;
		std::vector<const char*> shaderNames;

		union {
			// Raster Shading
			struct {
				
			};

			// Light Shading
			struct {
				int lightSamples[2];
				int shaderIndex;
				float causticsRatio; // Still unbiased
				bool lightPathFlags[4];
			};

			// Path Shading
			struct {
				bool specularOnly;
				bool pathPathFlags[4];
			};
		};
	};
}