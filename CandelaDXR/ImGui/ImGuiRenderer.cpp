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

	if (ImGui::DragInt("time", &timeMs, 1.f, 0, 2147483647))
	{
		renderer.getRendererTime().stop();
		renderer.getRendererTime().setElapsedTime(timeMs);
		changed = true;
	}

	ImGui::Text("Time: %u", renderer.getRendererTime().getTimeMs());

	for (auto& animRecord : renderer.getAnimationRecords())
	{
		ImGui::PushID(&animRecord);
		if (ImGui::Checkbox("Enabled", &animRecord.enabled))
			changed = true;
		ImGui::SameLine();
		ImGui::Text(animRecord.name.c_str());
		ImGui::PopID();
	}
}

bool ImGuiRenderer::hasChanged() const
{
	return changed;
}
