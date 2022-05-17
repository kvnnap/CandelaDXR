#include <filesystem>

#include "imgui/imgui.h"

#include "Renderer/RasterShading.h"
#include "Renderer/LightTracingShading.h"
#include "Renderer/PathTracingShading.h"
#include "Renderer/RasterRTShadowsShading.h"

#include "ImGuiShading.h"

using std::uint32_t;
using std::filesystem::path;

using candela::renderer::RasterShading;
using candela::renderer::LightTracingShading;
using candela::renderer::PathTracingShading;
using candela::renderer::RasterRTShadowsShading;

using candela::renderer::imgui::ImGuiShading;

ImGuiShading::ImGuiShading(IDrawable* drawable)
	: drawable(drawable), initialised(), changed(), enabled(drawable->isEnabled())
{
}

void ImGuiShading::drawUi()
{
	changed = false;
	ImGui::PushID(drawable);
	drawable->accept(this); // Calls the appropriate visit method
	changed |= ImGui::Checkbox("Enabled", &enabled);
	drawable->setEnabled(enabled);
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

void ImGuiShading::visit(RasterRTShadowsShading* rasterShader)
{
	if (!initialised)
	{
		lightType = rasterShader->getLightType();
	}
	ImGui::Text("RasterRTShadowsShading");
	if (ImGui::Checkbox("Area Light", &lightType))
	{
		rasterShader->setLightType(lightType ? 1 : 0);
		changed = true;
	}
}

void ImGuiShading::visit(LightTracingShading* lightTracingShader)
{
	if (!initialised)
	{
		memcpy(lightSamples, &lightTracingShader->getLightSamples(), 2 * sizeof(int));
		bounces[0] = static_cast<int>(lightTracingShader->getMinBounces());
		bounces[1] = static_cast<int>(lightTracingShader->getMaxBounces());
		shaderIndex = static_cast<int>(lightTracingShader->getCurrentShaderIndex());
		causticsRatio = lightTracingShader->getCausticsRatio();
		memset(lightPathFlags, true, sizeof(lightPathFlags));
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

	if (processPathLength(bounces))
	{
		lightTracingShader->setMinBounces(bounces[0]);
		lightTracingShader->setMaxBounces(bounces[1]);
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

	uint32_t pathFlags;
	if (processPathFilter(pathFlags, lightPathFlags))
	{
		lightTracingShader->setPathFilter(pathFlags);
		changed = true;
	}
}

void ImGuiShading::visit(PathTracingShading* pathTracingShader)
{
	if (!initialised)
	{
		specularOnly = pathTracingShader->getSpecularOnly();
		pathBounces[0] = static_cast<int>(pathTracingShader->getMinBounces());
		pathBounces[1] = static_cast<int>(pathTracingShader->getMaxBounces());
		memset(pathPathFlags, true, sizeof(pathPathFlags));
	}

	ImGui::Text("PathTracingShading");
	if (ImGui::Checkbox("Specular Only", &specularOnly))
	{
		pathTracingShader->setSpecularOnly(specularOnly);
		changed = true;
	}

	if (processPathLength(pathBounces))
	{
		pathTracingShader->setMinBounces(pathBounces[0]);
		pathTracingShader->setMaxBounces(pathBounces[1]);
		changed = true;
	}

	uint32_t pathFlags;
	if (processPathFilter(pathFlags, pathPathFlags))
	{
		pathTracingShader->setPathFilter(pathFlags);
		changed = true;
	}
}

bool ImGuiShading::processPathFilter(uint32_t &pathFlags, bool (&cPathFlags)[4])
{
	bool lChanged = false;
	lChanged |= ImGui::Checkbox("Direct Light Flag", &cPathFlags[0]);
	lChanged |= ImGui::Checkbox("Reflect Flag", &cPathFlags[1]);
	lChanged |= ImGui::Checkbox("Refract Flag", &cPathFlags[2]);
	lChanged |= ImGui::Checkbox("Diffuse Flag", &cPathFlags[3]);
	pathFlags = 0;
	if (cPathFlags[0]) pathFlags |= PathInteraction::Light;
	if (cPathFlags[1]) pathFlags |= PathInteraction::Reflect;
	if (cPathFlags[2]) pathFlags |= PathInteraction::Refract;
	if (cPathFlags[3]) pathFlags |= PathInteraction::Diffuse;
	return lChanged;
}

bool ImGuiShading::processPathLength(int(&bounces)[2])
{
	auto prevMin = bounces[0];
	if (ImGui::DragInt2("Path Length", &bounces[0], 1.f, 0, 16384))
	{
		if (bounces[0] > bounces[1])
		{
			if (prevMin == bounces[0])
				bounces[0] = bounces[1];
			else
				bounces[1] = bounces[0];
		}

		return true;
	}
	return false;
}
