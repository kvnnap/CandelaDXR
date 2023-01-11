#include "imgui/imgui.h"

#include "ImGuiRenderer.h"

#include "Renderer/Renderer.h"


using candela::renderer::imgui::ImGuiRenderer;

ImGuiRenderer::ImGuiRenderer(Renderer& renderer)
	: renderer(renderer), changed(), animating(), shaderAccumulation(), timeMs()
{
	animating = renderer.getRendererTime().isRunning();
}

void ImGuiRenderer::drawUi()
{
	changed = false;
	auto &rTime = renderer.getRendererTime();
	animating = rTime.isRunning();
	shaderAccumulation = renderer.getShaderAccumulation();
	
	if (ImGui::Button("Record"))
		renderer.getAnimationSequencer().setEnabled(true);

	if (ImGui::Checkbox("Animation", &animating))
	{
		if (animating)
			rTime.start(false);
		else
			rTime.stop();
		changed = true;
	}

	if (ImGui::Checkbox("Shader Accum", &shaderAccumulation))
		renderer.setShaderAccumulation(shaderAccumulation);

	if (ImGui::DragInt("time", &timeMs, 1.f, 0, 2147483647))
	{
		rTime.stop();
		rTime.setElapsedTime(timeMs);
		changed = true;
	}

	ImGui::Text("Time: %u", rTime.getTimeMs());

	auto& animSeq = renderer.getAnimationSequencer();

	ImGui::Text("FramesPerAnim: %u", animSeq.getFramesPerAnimation());
	ImGui::Text("TimeDeltaMs: %u", animSeq.getTimeDeltaMs());
	ImGui::Text("MaxTimeMs: %u", animSeq.getMaxTimeMs());

	for (auto& animRecord : renderer.getAnimationRecords())
	{
		ImGui::PushID(&animRecord);
		if (ImGui::Checkbox("Enabled", &animRecord.enabled))
			changed = true;
		ImGui::SameLine();
		ImGui::Text(animRecord.name.c_str());
		ImGui::PopID();
	}

	ImGui::Text("Scene Cameras");
	for (const auto& camera : renderer.getScene().getCameras())
	{
		if (ImGui::Button(camera.getName().c_str()))
			renderer.setCameraCopy(camera);
	}

	/*if (ImGui::ListBox("Shader", &shaderIndex, shaderNames.data(), static_cast<int>(shaderNames.size())))
	{
		lightTracingShader->setCurrentShaderIndex(static_cast<uint32_t>(shaderIndex));
		currentComponent = ltShaderInfo[shaderIndex].component;
		changed = true;
	}*/
	
}

bool ImGuiRenderer::hasChanged() const
{
	return changed;
}
