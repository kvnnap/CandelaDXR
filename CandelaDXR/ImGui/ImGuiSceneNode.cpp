#include "imgui/imgui.h"

#include "ImGuiSceneNode.h"
#include "Mathematics/Utils.h"
#include "Mathematics/Constants.h"

#include <algorithm>

using candela::renderer::imgui::ImGuiSceneNode;
using candela::renderer::RendererTime;
using candela::scene::SceneNode;
using candela::scene::Scene;
using candela::mathematics::QuaternionToRotationXYZ;
using candela::mathematics::Vector3;
using candela::mathematics::Vector;

ImGuiSceneNode::ImGuiSceneNode(SceneNode &p_sceneNode, const RendererTime& rendererTime)
	: sceneNode(p_sceneNode), rendererTime(rendererTime), transformComponents{}, changed(), useModelCentre()
{
	transformComponents.setFromMatrix(sceneNode.Transform);
	for (auto& sceneChild : p_sceneNode.Children)
		children.emplace_back(*sceneChild, rendererTime);
}

void ImGuiSceneNode::drawUi()
{
	ImGui::PushID(this);
	if (rendererTime.isRunning())
	{
		ImGui::Text("Animated");
		ImGui::PopID();
		return;
	}

	if (sceneNode.isLeaf() || ImGui::TreeNode(sceneNode.NodeName.c_str()))
	{
		if (sceneNode.isLeaf())
			ImGui::Text(sceneNode.NodeName.c_str());

		changed = ImGui::DragFloat3("Position", &transformComponents.Translate.m128_f32[0], 0.01f);
		changed |= ImGui::DragFloat3("Rotation", &transformComponents.Rotate.m128_f32[0], 0.01f);
		changed |= ImGui::DragFloat3("Scale", &transformComponents.Scale.m128_f32[0], 0.01f, 0.f, 1000.f);
		changed |= ImGui::Checkbox("Rotate/scale around Model Centre", &useModelCentre);

		for (auto& nodeChild : children)
			nodeChild.drawUi();
		if (!sceneNode.isLeaf())
			ImGui::TreePop();
	}

	ImGui::PopID();

	if (changed || (useModelCentre && hasChanged()))
	{
		sceneNode.Transform = useModelCentre ? 
			transformComponents.transform(sceneNode.getCentrePosition()) : 
			transformComponents.transform();
	}
}

bool ImGuiSceneNode::hasChanged() const
{
	return changed || std::any_of(children.begin(), children.end(), [](const ImGuiSceneNode& s) { return s.hasChanged(); });
}
