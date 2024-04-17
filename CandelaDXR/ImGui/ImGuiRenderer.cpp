#include "imgui/imgui.h"

#include "ImGuiRenderer.h"

#include "Renderer/Renderer.h"
#include "Shader/AccumulatorShader.hlsli"

using candela::renderer::imgui::ImGuiRenderer;
using std::to_string;

ImGuiRenderer::ImGuiRenderer(Renderer& renderer)
	: renderer(renderer), changed(), animating(), shaderAccumulation(), showLights(), timeMs()
{
	animating = renderer.getRendererTime().isRunning();

	// Load initial Post Proc state
	auto& ppParams = renderer.getPostProcParams();
	exposureFlag	=	ACC_IS_SET(ACC_EXPOSURE,		ppParams.Flags);
	selectedToneMapper = 0;
	if (ACC_IS_SET(ACC_TONEMAP, ppParams.Flags))
		selectedToneMapper = 1;
	else if (ACC_IS_SET(ACC_TONEMAP_ACES, ppParams.Flags))
		selectedToneMapper = 2;
	linearToSrgb	=	ACC_IS_SET(ACC_LINEARTOSRGB,	ppParams.Flags);
	exposure = ppParams.Exposure;
}

void ImGuiRenderer::drawUi()
{
	changed = false;
	auto &rTime = renderer.getRendererTime();
	animating = rTime.isRunning();
	shaderAccumulation = renderer.getShaderAccumulation();
	
	// Exposure and tonemapping stuff
	bool ppChange = false;
	ppChange |= ImGui::Checkbox("Exposure", &exposureFlag);
	if (exposureFlag)
		ppChange |= ImGui::DragFloat("Exposure Val", &exposure, 0.05f, -10.f, 10.f);
	static const char* items[]{ "None", "Reinhard", "ACES"};
	ppChange |= ImGui::Combo("ToneMapper", &selectedToneMapper, items, 3);
	ppChange |= ImGui::Checkbox("LinearToSrgb", &linearToSrgb);
	if (ppChange)
		renderer.setPostProcParams({
			  (exposureFlag	? ACC_EXPOSURE		: 0)
			| (selectedToneMapper == 1 ? ACC_TONEMAP : selectedToneMapper == 2 ? ACC_TONEMAP_ACES : 0)
			| (linearToSrgb ? ACC_LINEARTOSRGB	: 0),
			exposure
		});

	// End exposure stuff

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
		if (ImGui::Button(camera.Camera.getName().c_str()))
		{
			auto camCopy = camera.Camera;
			camCopy.transform(camera.Node->getTransform());
			renderer.setCameraCopy(camCopy);
		}
	}

	const auto& extLights = renderer.getScene().getExternalLights();
	ImGui::Text(("Scene Lights (External): " + to_string(extLights.size())).c_str());
	if (!extLights.empty())
	{
		ImGui::Checkbox("Show Lights", &showLights);
		if (showLights)
		{
			for (const auto& lightNode : renderer.getScene().getExternalLights())
			{
				ImGui::Text(("Name" + lightNode.Node->NodeName).c_str());
				ImGui::Text(("Type" + to_string(lightNode.Light.Type)).c_str());
				ImGui::Text("Value: {%f, %f, %f}", lightNode.Light.Diffuse.x, lightNode.Light.Diffuse.y, lightNode.Light.Diffuse.z);
			}
		}
	}
	

	ImGui::Text("Profiling");
	for (const auto& profilingItem : renderer.getProfilingData())
		ImGui::Text("%s: %.2f ms",profilingItem.ProfileName.c_str(), profilingItem.TimeMs);

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
