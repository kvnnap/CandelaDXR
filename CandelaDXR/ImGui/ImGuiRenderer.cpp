#include "imgui/imgui.h"

#include "ImGuiRenderer.h"

#include "Renderer/Renderer.h"


using candela::renderer::imgui::ImGuiRenderer;

ImGuiRenderer::ImGuiRenderer(Renderer& renderer)
	: changed(), renderer(renderer)
{
	animating = renderer.getAnimationEnabled();
}

void ImGuiRenderer::drawUi()
{
	changed = false;
	if (ImGui::Checkbox("Animation", &animating))
	{
		renderer.setAnimationEnabled(animating);
		changed = true;
	}

	ImGui::Text("Time: %u", renderer.getRendererTime());
}

bool ImGuiRenderer::hasChanged() const
{
	return changed;
}
