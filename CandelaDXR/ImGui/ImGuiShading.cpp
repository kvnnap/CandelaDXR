#include "ImGuiShading.h"

#include "imgui/imgui.h"

#include "Renderer/RasterShading.h"
#include "Renderer/LightTracingShading.h"

#include <filesystem>

using std::uint32_t;
using std::filesystem::path;

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
	{
		memcpy(lightSamples, &lightTracingShader->getLightSamples(), 2 * sizeof(int));
		shaderIndex = static_cast<int>(lightTracingShader->getCurrentShaderIndex());
		causticsRatio = lightTracingShader->getCausticsRatio();
		lightFlag = reflectFlag = refractFlag = diffuseFlag = true;
		for (const auto& shaderName : lightTracingShader->getShaderPaths())
			shaderStrNames.push_back(path(shaderName).filename().replace_extension().string());
		for (const auto& shaderName : shaderStrNames)
			shaderNames.push_back(shaderName.c_str());
	}
	ImGui::Text("LightTracingShading");
	if (ImGui::DragInt2("LightSamples", &lightSamples[0], 1.f, 0, 4096))
	{
		lightTracingShader->setLightSamples({
			static_cast<uint32_t>(lightSamples[0]),
			static_cast<uint32_t>(lightSamples[1])
		});
		changed = true;
	}

	if (ImGui::ListBox("Shader", &shaderIndex, shaderNames.data(), static_cast<int>(shaderNames.size())))
	{ 
		lightTracingShader->setCurrentShaderIndex(static_cast<uint32_t>(shaderIndex));
		changed = true;
	}

	if (shaderIndex == 1 && ImGui::DragFloat("Caustics Ratio", &causticsRatio, 0.01f, 0.f, 1.f))
	{
		lightTracingShader->setCausticsRatio(causticsRatio);
		changed = true;
	}

	changed |= ImGui::Checkbox("Direct Light Flag", &lightFlag);
	changed |= ImGui::Checkbox("Reflect Flag", &reflectFlag);
	changed |= ImGui::Checkbox("Refract Flag", &refractFlag);
	changed |= ImGui::Checkbox("Diffuse Flag", &diffuseFlag);
	std::uint32_t pathFlags = 0;
	if (lightFlag) pathFlags |= PathInteraction::Light;
	if (reflectFlag) pathFlags |= PathInteraction::Reflect;
	if (refractFlag) pathFlags |= PathInteraction::Refract;
	if (diffuseFlag) pathFlags |= PathInteraction::Diffuse;
	if (changed)
		lightTracingShader->setPathFilter(pathFlags);
}
