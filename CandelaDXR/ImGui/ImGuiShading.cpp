#include <filesystem>

#include "imgui/imgui.h"

#include "Renderer/RasterShading.h"
#include "Renderer/LightTracingShading.h"
#include "Renderer/LTOptimisedComponent.h"
#include "Renderer/LTRasterGuidedShading.h"
#include "Renderer/PathTracingShading.h"
#include "Renderer/RasterRTShadowsShading.h"
#include "Renderer/DenoiserShading.h"
#include "Renderer/ExternalObjectDebugShading.h"

#include "ImGuiShading.h"

using std::uint32_t;
using std::filesystem::path;

using candela::renderer::RasterShading;
using candela::renderer::LightTracingShading;
using candela::renderer::PathTracingShading;
using candela::renderer::RasterRTShadowsShading;
using candela::renderer::DenoiserShading;

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
	auto enabledChanged = ImGui::Checkbox("Enabled", &enabled);
	changed |= enabledChanged;
	if (enabledChanged)
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
	if (!initialised)
	{
		computeRadiance = rasterShader->getComputeRadiance();
		computeEmissiveIfRadOff = rasterShader->getComputeEmissiveIfRadOff();
	}
	ImGui::Text("RasterShading");
	if (ImGui::Checkbox("Compute Radiance", &computeRadiance))
	{
		rasterShader->setComputeRadiance(computeRadiance);
		changed = true;
	}
	if (!computeRadiance)
	{
		if (ImGui::Checkbox("Compute Emissive", &computeEmissiveIfRadOff))
		{
			rasterShader->setComputeEmissiveIfRadOff(computeEmissiveIfRadOff);
			changed = true;
		}
	}
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

void ImGuiShading::visit(DenoiserShading* denShading)
{
	ImGui::Text("DenoiserShading - CS");
	
	auto& common = denShading->getCommonSettings();
	changed |= ImGui::DragFloat("Denoising Range", &common.denoisingRange, 1.f, 1.f, 524031.f);
	changed |= ImGui::DragFloat("Disocclusion Threshold", &common.disocclusionThreshold, 0.0001f, 0.0025f, 0.0150f);
	changed |= ImGui::DragFloat("Split Screen", &common.splitScreen, 0.01f, 0.f, 1.f);

	auto denSelected = denShading->getDenoiserSelected();
	bool relax = denSelected;
	if (ImGui::Checkbox("ReLax", &relax))
	{
		changed = true;
		denShading->setDenoiserSelected(relax ? 1u : 0u);
	}

	auto denCaustics = denShading->getDenoiseCaustics();
	bool denCBool = denCaustics;
	if (ImGui::Checkbox("Denoise Caustics", &denCBool))
	{
		changed = true;
		denShading->setDenoiseCaustics(denCBool ? 1u : 0u);
	}

	if (denSelected == 0)
	{
		ImGui::Text("DenoiserShading - RS");

		auto& reblur = denShading->getReblurSettings();
		changed |= ImGui::DragInt("Max Accum Frame", reinterpret_cast<int*>(&reblur.maxAccumulatedFrameNum), 1.f, 0, 1000);
		changed |= ImGui::DragInt("Max Fast Accum Frame", reinterpret_cast<int*>(&reblur.maxFastAccumulatedFrameNum), 1.f, 0, reblur.maxAccumulatedFrameNum - 1);
		changed |= ImGui::DragInt("History Fix frame num", reinterpret_cast<int*>(&reblur.historyFixFrameNum), 1.f, 0, reblur.maxFastAccumulatedFrameNum - 1);
		changed |= ImGui::DragFloat("diffusePrepassBlurRadius", &reblur.diffusePrepassBlurRadius, 1.f, 1.f, 100.f);
		changed |= ImGui::DragFloat("blurRadius", &reblur.blurRadius, 1.f, 1.f, 100.f);
		changed |= ImGui::DragFloat("historyFixStrideBetweenSamples", &reblur.historyFixStrideBetweenSamples, 1.f, 1.f, 100.f);
		changed |= ImGui::DragFloat("stabilizationStrength", &reblur.stabilizationStrength, 0.01f, 0.01f, 1.f);

		// AntiLag
		ImGui::Text("AntiLagHitDist");
		changed |= ImGui::DragFloat("hitDistanceAntilagPower", &reblur.antilagSettings.hitDistanceAntilagPower, 0.01f, 0.01f, 1.f);
		changed |= ImGui::DragFloat("hitDistanceSigmaScale", &reblur.antilagSettings.hitDistanceSigmaScale, 0.01f, 0.01f, 2.f);
		changed |= ImGui::DragFloat("luminanceAntilagPower", &reblur.antilagSettings.luminanceAntilagPower, 0.01f, 0.01f, 0.5f);
		changed |= ImGui::DragFloat("luminanceSigmaScale", &reblur.antilagSettings.luminanceSigmaScale, 0.01f, 0.01f, 2.f);
	}
}

void ImGuiShading::visit(ExternalObjectDebugShading* eoDebug)
{
	if (!initialised)
	{
		displaySceneAabb = eoDebug->getDisplaySceneAabb();
	}

	ImGui::Text("ExternalObjectDebugShading");

	if (ImGui::Checkbox("Scene AABB", &displaySceneAabb))
	{
		eoDebug->setDisplaySceneAabb(displaySceneAabb);
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
		causticsRatio = -1.f;
		filterSize = -1;
		causticsBlurSize = lightTracingShader->getCausticsBlurSize();
		memset(lightPathFlags, true, sizeof(lightPathFlags));
		for (const auto& ltShadInfo : lightTracingShader->getLTShaderInfo())
		{
			auto& localLT = ltShaderInfo.emplace_back<LTShaderInfo>({});
			localLT.shaderStrName = path(*ltShadInfo.shaderPath).filename().replace_extension().string();
			localLT.component = ltShadInfo.component;
			shaderNames.push_back(localLT.shaderStrName.c_str());
		}

		currentComponent = ltShaderInfo[shaderIndex].component;
		seperateCaustics = lightTracingShader->getSeperateCaustics();
		allowClearCaustics = lightTracingShader->getAllowClearCaustics();
		rangeBits = lightTracingShader->getRangeBits();
	}
	ImGui::Text("LightTracingShading");
	
	if (ImGui::DragInt("ConvRangeBits", &rangeBits, 1.f, 0, 32))
	{
		lightTracingShader->setRangeBits(rangeBits);
		changed = true;
	}

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
		currentComponent = ltShaderInfo[shaderIndex].component;
		changed = true;
	}

	if (currentComponent)
		currentComponent->accept(this);

	uint32_t pathFlags;
	if (processPathFilter(pathFlags, lightPathFlags))
	{
		lightTracingShader->setPathFilter(pathFlags);
		changed = true;
	}

	if (ImGui::Checkbox("Seperate Caustics", &seperateCaustics))
	{
		lightTracingShader->seperateCaustics(seperateCaustics ? 1u : 0u);
		changed = true;
	}

	if (seperateCaustics && ImGui::DragInt("Caustics Blur Size", &causticsBlurSize, 2.f, 1, FilterComputeShader::MaxSize))
	{
		lightTracingShader->setCausticsBlurSize(causticsBlurSize);
		causticsBlurSize = lightTracingShader->getCausticsBlurSize();
	}

	if (ImGui::Checkbox("Allow Clearing Caustics", &allowClearCaustics))
	{
		lightTracingShader->setAllowClearCautics(allowClearCaustics);
		changed = true;
	}
}

void ImGuiShading::visit(LTOptimisedComponent* ltComponent)
{
	if (causticsRatio == -1.f)
		causticsRatio = ltComponent->getCausticsRatio();

	if (ImGui::DragFloat("Caustics Ratio", &causticsRatio, 0.01f, 0.f, 1.f))
	{
		ltComponent->setCausticsRatio(causticsRatio);
		changed = true;
	}
}

void ImGuiShading::visit(LTRasterGuidedShading* ltRasterComponent)
{
	if (filterSize == -1)
	{
		distanceMetricMode = ltRasterComponent->getDistanceMetricMode();
		filterSize = ltRasterComponent->getFilterSize();
	}

	const char* arr[] = { "Distance", "Distance + Camera", "Distance+Cam+RT" };

	if (ImGui::ListBox("Metric", &distanceMetricMode, &arr[0], 3))
	{
		ltRasterComponent->setDistanceMetricMode(distanceMetricMode);
		changed = true;
	}

	if (ImGui::DragInt("Filter Size", &filterSize, 2.f, 1, FilterComputeShader::MaxSize))
	{
		ltRasterComponent->setFilterSize(filterSize);
		filterSize = ltRasterComponent->getFilterSize();
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
