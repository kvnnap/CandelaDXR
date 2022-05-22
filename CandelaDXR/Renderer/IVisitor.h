#pragma once

namespace candela::renderer
{
	class RasterShading;
	class LightTracingShading;
	class LTOptimisedComponent;
	class LTRasterGuidedShading;
	class PathTracingShading;
	class RasterRTShadowsShading;

	class IVisitor
	{
	public:
		virtual ~IVisitor() = default;
		virtual void visit(RasterShading*) = 0;
		virtual void visit(LightTracingShading*) = 0;
		virtual void visit(LTOptimisedComponent*) = 0;
		virtual void visit(LTRasterGuidedShading*) = 0;
		virtual void visit(PathTracingShading*) = 0;
		virtual void visit(RasterRTShadowsShading*) = 0;
	};
}
