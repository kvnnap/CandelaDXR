#pragma once

#include <vector>
#include <string>
#include <cstdint>

#include "Renderer/IVisitor.h"
#include "Renderer/IDrawable.h"
#include "Renderer/ILightTracingComponent.h"
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
		void visit(LTOptimisedComponent*) override;
		void visit(LTRasterGuidedShading*) override;
		void visit(PathTracingShading*) override;
		void visit(RasterRTShadowsShading*) override;
		void visit(DenoiserShading*) override;
		void visit(ExternalObjectDebugShading*) override;

	private:
		enum PathInteraction : std::uint32_t
		{
			Light = 1,
			Reflect = 2,
			Refract = 4,
			Diffuse = 8
		};

		static bool processPathFilter(uint32_t& pathFlags, bool(&cPathFlags)[4]);
		static bool processPathLength(int(&bounces)[2]);

		IDrawable *drawable;
		bool initialised;
		bool changed;
		bool enabled;

		struct LTShaderInfo 
		{
			std::string shaderStrName;
			ILightTracingComponent* component;
		};

		std::vector<LTShaderInfo> ltShaderInfo;
		std::vector<const char*> shaderNames;

		union {
			// Raster Shading
			struct {
				bool computeRadiance;
				bool computeEmissiveIfRadOff;
			};

			// Raster RT Shading
			struct {
				bool lightType;
			};

			// Light Shading
			struct {
				int lightSamples[2];
				int bounces[2];
				int shaderIndex;
				float causticsRatio; // Still unbiased
				bool lightPathFlags[4];
				ILightTracingComponent* currentComponent;
				int filterSize;
				int distanceMetricMode;
				bool seperateCaustics;
				bool allowClearCaustics;
			};

			// Path Shading
			struct {
				bool specularOnly;
				int pathBounces[2];
				bool pathPathFlags[4];
			};

			// External Debug
			struct {
				bool displaySceneAabb;
			};
		};
	};
}