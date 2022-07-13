#include "imgui/imgui.h"

#include "ImGuiRenderer.h"

#include "Renderer/Renderer.h"


using candela::renderer::imgui::ImGuiRenderer;

ImGuiRenderer::ImGuiRenderer(Renderer& renderer)
	: changed(), renderer(renderer), timeMs()
{
	animating = renderer.getRendererTime().isRunning();
}

void ImGuiRenderer::drawUi()
{
	changed = false;
	animating = renderer.getRendererTime().isRunning();
	if (ImGui::Checkbox("Animation", &animating))
	{
		if (animating)
			renderer.getRendererTime().start(false);
		else
			renderer.getRendererTime().stop();
		changed = true;
	}

	if (ImGui::DragInt("time", &timeMs))
	{
		renderer.getRendererTime().stop();
		renderer.getRendererTime().setElapsedTime(timeMs);
		changed = true;
	}

	ImGui::Text("Time: %u", renderer.getRendererTime().getTimeMs());
}

bool ImGuiRenderer::hasChanged() const
{
	return changed;
}
