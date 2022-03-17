#include "ImGuiShading.h"

#include "imgui/imgui.h"

#include "Renderer/RasterShading.h"
#include "Renderer/LightTracingShading.h"

using std::uint32_t;

using candela::renderer::RasterShading;
using candela::renderer::LightTracingShading;

using candela::renderer::imgui::ImGuiShading;

ImGuiShading::ImGuiShading(IDrawable* drawable)
	: drawable(drawable), initialised(), changed(), enabled(true)
{
}

void ImGuiShading::drawUi()
{
	changed = false;
	ImGui::PushID(drawable);
	drawable->accept(this); // Calls the appropriate visit method
	changed |= ImGui::Checkbox("Enabled", &enabled);
	ImGui::PopID();
	initialised = true;
}

bool ImGuiShading::hasChanged() const
{
	return changed;
}

bool ImGuiShading::isEnabled() const
{
	return enabled;
}

void ImGuiShading::visit(RasterShading* rasterShader)
{
	ImGui::Text("RasterShading");
}

void ImGuiShading::visit(LightTracingShading* lightTracingShader)
{
	if (!initialised)
		memcpy(lightSamples, &lightTracingShader->getLightSamples(), 2 * sizeof(int));
	ImGui::Text("LightTracingShading");
	if (ImGui::DragInt2("LightSamples", &lightSamples[0], 1.f, 0, 4096))
	{
		lightTracingShader->setLightSamples({
			static_cast<uint32_t>(lightSamples[0]),
			static_cast<uint32_t>(lightSamples[1])
		});
		changed = true;
	}
}
