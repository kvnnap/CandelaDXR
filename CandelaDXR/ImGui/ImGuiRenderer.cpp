#include "imgui/imgui.h"

#include "ImGuiRenderer.h"

#include "Renderer/Renderer.h"


using candela::renderer::imgui::ImGuiRenderer;

ImGuiRenderer::ImGuiRenderer(Renderer& renderer)
	: changed(), animating(), streamOutput(), renderer(renderer), timeMs()
{
	animating = renderer.getRendererTime().isRunning();
}

void ImGuiRenderer::drawUi()
{
	changed = false;
	auto &rTime = renderer.getRendererTime();
	animating = rTime.isRunning();
	streamOutput = renderer.isStreamingOutput();

	if (ImGui::Checkbox("Stream Output", &streamOutput))
	{
		renderer.setStreamOutput(streamOutput);
	}

	if (ImGui::Checkbox("Animation", &animating))
	{
		if (animating)
			rTime.start(false);
		else
			rTime.stop();
		changed = true;
	}

	if (ImGui::DragInt("time", &timeMs, 1.f, 0, 2147483647))
	{
		rTime.stop();
		rTime.setElapsedTime(timeMs);
		changed = true;
	}

	ImGui::Text("Time: %u", rTime.getTimeMs());

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
